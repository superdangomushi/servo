#include <iostream>
#include <opencv2/opencv.hpp>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <chrono>
#include <iomanip>
#include "../include/shm_data.h"

int main() {
    // 共有メモリの初期化
    const char* shm_name = "/aruco_data";
    int shm_fd = shm_open(shm_name, O_RDWR, 0666);
    if (shm_fd == -1) {
        std::cerr << "エラー: 共有メモリを開けませんでした。detect_humanL/R または marker_detect を先に実行してください。" << std::endl;
        return -1;
    }

    SharedMemoryData* shared_data = reinterpret_cast<SharedMemoryData*>(mmap(nullptr, sizeof(SharedMemoryData), 
                                                            PROT_READ, MAP_SHARED, shm_fd, 0));
    if (shared_data == MAP_FAILED) {
        std::cerr << "エラー: 共有メモリをマッピングできませんでした。" << std::endl;
        close(shm_fd);
        return -1;
    }

    cv::namedWindow("State Viewer", cv::WINDOW_NORMAL);
    cv::resizeWindow("State Viewer", 640, 480);

    while (true) {
        // 白い背景
        cv::Mat frame(480, 640, CV_8UC3, cv::Scalar(255, 255, 255));

        auto now = std::chrono::system_clock::now();
        double current_time = std::chrono::duration<double>(now.time_since_epoch()).count();
        double timeout = 1.0; // 1秒以上更新がなければ検出なしとみなす

        // Camera L (Blue)
        bool activeL = (current_time - shared_data->last_human_update_time_L) < timeout;
        int countL = activeL ? shared_data->human_count_L : 0;
        
        std::cout << "\033[2J\033[1;1H"; // 画面クリアとカーソル移動
        
        if (activeL && countL > 0) {
            std::cout << "L : ";
            for (int i = 0; i < countL; i++) {
                HumanPoseData& human = shared_data->humans_L[i];
                bool hasRight = (human.right_shoulder[0] != -1);
                bool hasLeft = (human.left_shoulder[0] != -1);

                if (hasRight) {
                    cv::circle(frame, cv::Point(human.right_shoulder[0], human.right_shoulder[1]), 8, cv::Scalar(255, 0, 0), -1);
                    cv::putText(frame, "L" + std::to_string(i) + "R", cv::Point(human.right_shoulder[0], human.right_shoulder[1]), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0), 1);
                    std::cout << "Right_shoulder(" << std::fixed << std::setprecision(1) << human.right_shoulder[0] << "," << human.right_shoulder[1] << ")";
                }
                
                if (hasRight && hasLeft) std::cout << ",";

                if (hasLeft) {
                    cv::circle(frame, cv::Point(human.left_shoulder[0], human.left_shoulder[1]), 8, cv::Scalar(255, 0, 0), -1);
                    cv::putText(frame, "L" + std::to_string(i) + "L", cv::Point(human.left_shoulder[0], human.left_shoulder[1]), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0), 1);
                    std::cout << "Left_shoulder(" << std::fixed << std::setprecision(1) << human.left_shoulder[0] << "," << human.left_shoulder[1] << ")";
                }
                if (i < countL - 1) std::cout << " ";
            }
            std::cout << std::endl;
        } else {
            std::cout << "L : not found" << std::endl;
        }

        // Camera R (Red)
        bool activeR = (current_time - shared_data->last_human_update_time_R) < timeout;
        int countR = activeR ? shared_data->human_count_R : 0;

        if (activeR && countR > 0) {
            std::cout << "R : ";
            for (int i = 0; i < countR; i++) {
                HumanPoseData& human = shared_data->humans_R[i];
                bool hasRight = (human.right_shoulder[0] != -1);
                bool hasLeft = (human.left_shoulder[0] != -1);

                if (hasRight) {
                    cv::circle(frame, cv::Point(human.right_shoulder[0], human.right_shoulder[1]), 8, cv::Scalar(0, 0, 255), -1);
                    cv::putText(frame, "R" + std::to_string(i) + "R", cv::Point(human.right_shoulder[0], human.right_shoulder[1]), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0), 1);
                    std::cout << "Right_shoulder(" << std::fixed << std::setprecision(1) << human.right_shoulder[0] << "," << human.right_shoulder[1] << ")";
                }

                if (hasRight && hasLeft) std::cout << ",";

                if (hasLeft) {
                    cv::circle(frame, cv::Point(human.left_shoulder[0], human.left_shoulder[1]), 8, cv::Scalar(0, 0, 255), -1);
                    cv::putText(frame, "R" + std::to_string(i) + "L", cv::Point(human.left_shoulder[0], human.left_shoulder[1]), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0), 1);
                    std::cout << "Left_shoulder(" << std::fixed << std::setprecision(1) << human.left_shoulder[0] << "," << human.left_shoulder[1] << ")";
                }
                if (i < countR - 1) std::cout << " ";
            }
            std::cout << std::endl;
        } else {
            std::cout << "R : not found" << std::endl;
        }

        cv::imshow("State Viewer", frame);
        if (cv::waitKey(30) == 'q') break;
    }

    munmap(shared_data, sizeof(SharedMemoryData));
    close(shm_fd);
    return 0;
}
