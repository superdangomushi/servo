#include <iostream>
#include <vector>
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <ctime>
#include <chrono>
#include "../include/shm_data.h"
#include "../include/human_tracker.h"

// OpenPose MobileNet (COCO) Keypoints mapping
// 0: Nose, 1: Neck, 2: RShoulder, 3: RElbow, 4: RWrist, 
// 5: LShoulder, 6: LElbow, 7: LWrist, 8: RHip, 9: RKnee, 
// 10: RAnkle, 11: LHip, 12: LKnee, 13: LAnkle, 14: REye, 
// 15: LEye, 16: REar, 17: LEar, 18: Background
const int KEYPOINT_RIGHT_SHOULDER = 2;
const int KEYPOINT_LEFT_SHOULDER = 5;

// ヒートマップからピーク（極大値）を検出する関数
std::vector<cv::Point> findPeaks(const cv::Mat& heatMap, float threshold) {
    std::vector<cv::Point> peaks;
    for (int y = 1; y < heatMap.rows - 1; y++) {
        const float* ptr = heatMap.ptr<float>(y);
        const float* ptr_up = heatMap.ptr<float>(y - 1);
        const float* ptr_down = heatMap.ptr<float>(y + 1);
        
        for (int x = 1; x < heatMap.cols - 1; x++) {
            float val = ptr[x];
            if (val > threshold) {
                if (val >= ptr[x-1] && val >= ptr[x+1] &&
                    val >= ptr_up[x] && val >= ptr_down[x]) {
                    peaks.push_back(cv::Point(x, y));
                }
            }
        }
    }
    return peaks;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <camera_path_or_id>" << std::endl;
        return -1;
    }

    // カメラデバイスのパスまたはIDを取得
    std::string camera_path;
    int camera_id = 0;
    bool use_camera_id = false;

    std::string arg = argv[1];
    if (std::all_of(arg.begin(), arg.end(), ::isdigit)) {
        camera_id = std::stoi(arg);
        use_camera_id = true;
    } else {
        camera_path = arg;
        use_camera_id = false;
    }

    // 共有メモリの初期化
    const char* shm_name = "/aruco_data"; // 同じ共有メモリを使用
    int shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        std::cerr << "エラー: 共有メモリを作成できませんでした。" << std::endl;
        return -1;
    }

    // 共有メモリのサイズを設定
    if (ftruncate(shm_fd, sizeof(SharedMemoryData)) == -1) {
        std::cerr << "エラー: 共有メモリのサイズを設定できませんでした。" << std::endl;
        close(shm_fd);
        return -1;
    }

    // 共有メモリをマッピング
    SharedMemoryData* shared_data = reinterpret_cast<SharedMemoryData*>(mmap(nullptr, sizeof(SharedMemoryData), 
                                                            PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0));
    if (shared_data == MAP_FAILED) {
        std::cerr << "エラー: 共有メモリをマッピングできませんでした。" << std::endl;
        close(shm_fd);
        return -1;
    }

    // Webカメラを開く
    cv::VideoCapture cap;
    if (use_camera_id) {
        cap.open(camera_id, cv::CAP_V4L2);
    } else {
        cap.open(camera_path, cv::CAP_V4L2);
    }

    if (!cap.isOpened()) {
        std::cerr << "エラー: カメラを開けませんでした。" << std::endl;
        munmap(shared_data, sizeof(SharedMemoryData));
        close(shm_fd);
        return -1;
    }
    
    std::cout << "Camera backend: " << cap.getBackendName() << std::endl;

    // OpenCV DNNでPose Estimationを行うための準備
    // OpenPose MobileNetモデル (TensorFlow) を使用
    std::string modelFile = "graph_opt.pb";
    
    // モデルファイルの存在確認
    if (access(modelFile.c_str(), F_OK) == -1) {
        std::cerr << "エラー: モデルファイルが見つかりません。" << std::endl;
        std::cerr << "make install を実行してモデルをダウンロードしてください。" << std::endl;
        return -1;
    }

    cv::dnn::Net net = cv::dnn::readNetFromTensorflow(modelFile);
    net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
    net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);

    // ウィンドウサイズを小さく設定
    cv::namedWindow("Human Detection R", cv::WINDOW_NORMAL);
    cv::resizeWindow("Human Detection R", 320, 240);

    HumanTracker tracker;

    while (true) {
        cv::Mat frame;
        cap >> frame;
        if (frame.empty()) break;

        // DNNへの入力を作成
        // OpenPose MobileNet (TensorFlow) の前処理
        // 参照元のPythonコードでは scale=1.0, mean=127.5 となっているためそれに合わせる
        cv::Mat inputBlob = cv::dnn::blobFromImage(frame, 1.0, cv::Size(368, 368), cv::Scalar(127.5, 127.5, 127.5), true, false);

        net.setInput(inputBlob);
        cv::Mat result = net.forward();

        // 結果の解析
        int nParts = 18;
        int H = result.size[2];
        int W = result.size[3];
        
        // 各パーツのピークを検出
        std::vector<std::vector<cv::Point>> allPeaks(nParts);
        for (int n = 0; n < nParts; n++) {
            cv::Mat heatMap(H, W, CV_32F, result.ptr(0, n));
            std::vector<cv::Point> peaks = findPeaks(heatMap, 0.1);
            
            // スケールバック
            for (auto& p : peaks) {
                p.x = (frame.cols * p.x) / W;
                p.y = (frame.rows * p.y) / H;
                allPeaks[n].push_back(p);
            }
        }

        // 人間のグルーピング (簡易版: 肩のペアリング)
        std::vector<HumanPoseData> detectedHumans;
        const int NOSE = 0;
        const int NECK = 1;
        const int RIGHT_SHOULDER = 2;
        const int RIGHT_ELBOW = 3;
        const int RIGHT_WRIST = 4;
        const int LEFT_SHOULDER = 5;
        const int LEFT_ELBOW = 6;
        const int LEFT_WRIST = 7;
        const int RIGHT_EYE = 14;
        const int LEFT_EYE = 15;
        const int RIGHT_EAR = 16;
        const int LEFT_EAR = 17;

        // 誤検知対策: 腕と顔が近くにある場合のみ肩として採用する
        auto isValidShoulder = [&](cv::Point shoulder, bool isRight) -> bool {
            double armDistThresh = frame.cols / 2.5;
            double faceDistThresh = frame.cols / 3.0;

            bool hasArm = false;
            if (isRight) {
                for (auto p : allPeaks[RIGHT_ELBOW]) if (cv::norm(shoulder - p) < armDistThresh) hasArm = true;
                if (!hasArm) for (auto p : allPeaks[RIGHT_WRIST]) if (cv::norm(shoulder - p) < armDistThresh * 1.5) hasArm = true;
            } else {
                for (auto p : allPeaks[LEFT_ELBOW]) if (cv::norm(shoulder - p) < armDistThresh) hasArm = true;
                if (!hasArm) for (auto p : allPeaks[LEFT_WRIST]) if (cv::norm(shoulder - p) < armDistThresh * 1.5) hasArm = true;
            }

            bool hasFace = false;
            for (auto p : allPeaks[NECK]) if (cv::norm(shoulder - p) < faceDistThresh) hasFace = true;
            if (!hasFace) for (auto p : allPeaks[NOSE]) if (cv::norm(shoulder - p) < faceDistThresh) hasFace = true;
            if (!hasFace) for (auto p : allPeaks[RIGHT_EYE]) if (cv::norm(shoulder - p) < faceDistThresh) hasFace = true;
            if (!hasFace) for (auto p : allPeaks[LEFT_EYE]) if (cv::norm(shoulder - p) < faceDistThresh) hasFace = true;
            if (!hasFace) for (auto p : allPeaks[RIGHT_EAR]) if (cv::norm(shoulder - p) < faceDistThresh) hasFace = true;
            if (!hasFace) for (auto p : allPeaks[LEFT_EAR]) if (cv::norm(shoulder - p) < faceDistThresh) hasFace = true;

            return hasArm && hasFace;
        };

        std::vector<cv::Point> rShoulders;
        for (auto p : allPeaks[RIGHT_SHOULDER]) {
            if (isValidShoulder(p, true)) rShoulders.push_back(p);
        }

        std::vector<cv::Point> lShoulders;
        for (auto p : allPeaks[LEFT_SHOULDER]) {
            if (isValidShoulder(p, false)) lShoulders.push_back(p);
        }
        
        std::vector<bool> lUsed(lShoulders.size(), false);

        // 右肩を基準に左肩を探す
        for (const auto& r : rShoulders) {
            HumanPoseData human;
            human.detected = true;
            human.right_shoulder[0] = r.x;
            human.right_shoulder[1] = r.y;
            human.left_shoulder[0] = -1;
            human.left_shoulder[1] = -1;

            int bestL = -1;
            double minDesc = 100000; // 大きな値

            for (size_t i = 0; i < lShoulders.size(); i++) {
                if (lUsed[i]) continue;
                
                double dist = cv::norm(r - lShoulders[i]);
                // 肩幅の閾値 (画面サイズによるが、とりあえず適当に)
                if (dist < frame.cols / 2) { 
                    if (dist < minDesc) {
                        minDesc = dist;
                        bestL = i;
                    }
                }
            }

            if (bestL != -1) {
                human.left_shoulder[0] = lShoulders[bestL].x;
                human.left_shoulder[1] = lShoulders[bestL].y;
                lUsed[bestL] = true;
            }
            detectedHumans.push_back(human);
        }

        // 使われなかった左肩を別の人間として追加
        for (size_t i = 0; i < lShoulders.size(); i++) {
            if (!lUsed[i]) {
                HumanPoseData human;
                human.detected = true;
                human.right_shoulder[0] = -1;
                human.right_shoulder[1] = -1;
                human.left_shoulder[0] = lShoulders[i].x;
                human.left_shoulder[1] = lShoulders[i].y;
                detectedHumans.push_back(human);
            }
        }

        // トラッカー更新
        tracker.update(detectedHumans);
        std::vector<HumanPoseData> trackedHumans = tracker.getResult();
        tracker.drawDebug(frame);

        // 描画と共有メモリへの書き込み (Camera R)
        shared_data->human_count_R = std::min((int)trackedHumans.size(), 10);
        
        // Use wall clock time for timestamp
        auto now = std::chrono::system_clock::now();
        double timestamp = std::chrono::duration<double>(now.time_since_epoch()).count();
        
        shared_data->last_human_update_time_R = timestamp;

        for (int i = 0; i < shared_data->human_count_R; i++) {
            shared_data->humans_R[i] = trackedHumans[i];
            shared_data->humans_R[i].timestamp = timestamp;

            // 描画
            cv::Point r(trackedHumans[i].right_shoulder[0], trackedHumans[i].right_shoulder[1]);
            cv::Point l(trackedHumans[i].left_shoulder[0], trackedHumans[i].left_shoulder[1]);

            if (r.x != -1) {
                cv::circle(frame, r, 8, cv::Scalar(0, 0, 255), -1);
                cv::putText(frame, "R" + std::to_string(i), r, cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255), 1);
            }
            if (l.x != -1) {
                cv::circle(frame, l, 8, cv::Scalar(0, 0, 255), -1);
                cv::putText(frame, "R" + std::to_string(i), l, cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255), 1);
            }
            if (r.x != -1 && l.x != -1) {
                cv::line(frame, r, l, cv::Scalar(255, 0, 0), 2);
            }
        }
        
        // 手首の描画 (全検出点)
        for (const auto& p : allPeaks[RIGHT_WRIST]) {
             cv::line(frame, p, cv::Point(p.x, std::max(0, p.y - 100)), cv::Scalar(0, 255, 255), 2);
             cv::putText(frame, "Hand", cv::Point(p.x, std::max(10, p.y - 105)), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 255), 1);
        }
        for (const auto& p : allPeaks[LEFT_WRIST]) {
             cv::line(frame, p, cv::Point(p.x, std::max(0, p.y - 100)), cv::Scalar(0, 255, 255), 2);
             cv::putText(frame, "Hand", cv::Point(p.x, std::max(10, p.y - 105)), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 255), 1);
        }

        std::cout << "Detected Humans (R): " << trackedHumans.size() << " (Locked ID: " << tracker.getLockedId() << ")" << std::endl;

        cv::imshow("Human Detection R", frame);

        if (cv::waitKey(1) == 'q') {
            break;
        }
    }

    cap.release();
    cv::destroyAllWindows();

    munmap(shared_data, sizeof(SharedMemoryData));
    close(shm_fd);

    return 0;
}
