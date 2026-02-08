#include <iostream>
#include <vector>
#include <opencv2/opencv.hpp>
#include <opencv2/aruco.hpp>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <ctime>
#include "../include/shm_data.h"

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
    const char* shm_name = "/aruco_data";
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

    // 共有メモリデータを初期化 (注意: 別のプロセスが既に動いている場合は初期化しない方が良いかもしれないが、ここでは簡易的に)
    // memset(shared_data, 0, sizeof(SharedMemoryData)); 
    // 既存のデータを消さないように、必要な部分だけ更新するか、起動時に一度だけクリアするロジックが必要。
    // 今回はとりあえずそのままにします。

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

    // ArUcoマーカーの辞書とパラメータを準備
    cv::Ptr<cv::aruco::Dictionary> dictionary = cv::aruco::getPredefinedDictionary(cv::aruco::DICT_4X4_50);
    cv::Ptr<cv::aruco::DetectorParameters> detectorParams = cv::aruco::DetectorParameters::create();

    // カメラの内部パラメータ（キャリブレーションで得られる値）
    double fx = 600.0, fy = 600.0;
    double cx = 320.0, cy = 240.0;
    cv::Mat cameraMatrix = (cv::Mat_<double>(3, 3) << fx, 0, cx, 0, fy, cy, 0, 0, 1);
    cv::Mat distCoeffs = cv::Mat::zeros(5, 1, CV_64F); // 歪み係数（今回はゼロと仮定）

    // ウィンドウサイズを小さく設定
    cv::namedWindow("AR Marker Detection", cv::WINDOW_NORMAL);
    cv::resizeWindow("AR Marker Detection", 320, 240);

    while (true) {
        cv::Mat frame;
        cap >> frame;
        if (frame.empty()) break;

        // マーカーを検出
        std::vector<int> markerIds;
        std::vector<std::vector<cv::Point2f>> markerCorners, rejectedCandidates;
        
        cv::aruco::detectMarkers(frame, dictionary, markerCorners, markerIds, detectorParams, rejectedCandidates);

        // 検出されたマーカーがあれば処理
        if (!markerIds.empty()) {
            // 検出したマーカーの輪郭を描画
            cv::aruco::drawDetectedMarkers(frame, markerCorners, markerIds);

            // 各マーカーの姿勢を推定
            std::vector<cv::Vec3d> rvecs, tvecs; // 回転ベクトルと平行移動ベクトル
            // 第2引数はマーカーの実際のサイズ(メートル単位)
            cv::aruco::estimatePoseSingleMarkers(markerCorners, 0.05, cameraMatrix, distCoeffs, rvecs, tvecs);

            // 共有メモリにマーカーデータを書き込み
            shared_data->marker_count = markerIds.size();
            
            // Use wall clock time for timestamp
            auto now = std::chrono::system_clock::now();
            double timestamp = std::chrono::duration<double>(now.time_since_epoch()).count();
            shared_data->last_marker_update_time = timestamp;
            
            for (size_t i = 0; i < markerIds.size() && i < 10; ++i) {
                shared_data->markers[i].id = markerIds[i];
                shared_data->markers[i].tvec[0] = tvecs[i][0];
                shared_data->markers[i].tvec[1] = tvecs[i][1];
                shared_data->markers[i].tvec[2] = tvecs[i][2];
                shared_data->markers[i].rvec[0] = rvecs[i][0];
                shared_data->markers[i].rvec[1] = rvecs[i][1];
                shared_data->markers[i].rvec[2] = rvecs[i][2];
                shared_data->markers[i].timestamp = shared_data->last_marker_update_time;
            }

            // 推定した姿勢（座標軸）を描画
            for (size_t i = 0; i < markerIds.size(); ++i) {
                cv::drawFrameAxes(frame, cameraMatrix, distCoeffs, rvecs[i], tvecs[i], 0.1);
                
                // IDと位置情報を表示（コンソールと共有メモリ両方に出力）
                std::cout << "ID: " << markerIds[i] 
                          << ", tvec: [" << tvecs[i][0] << ", " << tvecs[i][1] << ", " << tvecs[i][2] << "]"
                          << " (共有メモリに書き込み済み)" << std::endl;
            }
        } else {
            // マーカーが検出されなかった場合
            shared_data->marker_count = 0;
            auto now = std::chrono::system_clock::now();
            shared_data->last_marker_update_time = std::chrono::duration<double>(now.time_since_epoch()).count();
        }

        // 結果を表示
        cv::imshow("AR Marker Detection", frame);

        // 'q'キーが押されたらループを抜ける
        if (cv::waitKey(1) == 'q') {
            break;
        }
    }

    cap.release();
    cv::destroyAllWindows();

    // 共有メモリのクリーンアップ
    munmap(shared_data, sizeof(SharedMemoryData));
    close(shm_fd);
    // shm_unlink(shm_name); // 共有メモリセグメントを削除
    // NOTE: shm_unlink is not called here to avoid removing the shared memory segment
    // while other processes might still be using it. Cleanup should be handled separately.

    return 0;
}