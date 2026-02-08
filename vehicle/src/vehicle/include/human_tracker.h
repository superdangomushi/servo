#ifndef HUMAN_TRACKER_H
#define HUMAN_TRACKER_H

#include <vector>
#include <opencv2/opencv.hpp>
#include "shm_data.h"

struct TrackedHuman {
    int id;
    HumanPoseData data;
    cv::Point2f center;
    int consecutive_frames;
    int missing_frames;
};

class HumanTracker {
public:
    HumanTracker() : next_id(0), locked_id(-1) {}

    void update(const std::vector<HumanPoseData>& detections) {
        std::vector<bool> detection_used(detections.size(), false);
        
        // 1. Update existing tracks
        for (auto& track : tracks) {
            track.missing_frames++;
            
            int best_match = -1;
            double min_dist = 100.0; // Pixel distance threshold

            for (size_t j = 0; j < detections.size(); ++j) {
                if (detection_used[j]) continue;

                cv::Point2f det_center = getCenter(detections[j]);
                double dist = cv::norm(track.center - det_center);

                if (dist < min_dist) {
                    min_dist = dist;
                    best_match = j;
                }
            }

            if (best_match != -1) {
                track.data = detections[best_match];
                track.center = getCenter(detections[best_match]);
                track.consecutive_frames++;
                track.missing_frames = 0;
                detection_used[best_match] = true;
            } else {
                track.consecutive_frames = 0; // Reset consecutive count if missed
            }
        }

        // 2. Create new tracks for unused detections
        for (size_t j = 0; j < detections.size(); ++j) {
            if (!detection_used[j]) {
                TrackedHuman new_track;
                new_track.id = next_id++;
                new_track.data = detections[j];
                new_track.center = getCenter(detections[j]);
                new_track.consecutive_frames = 1;
                new_track.missing_frames = 0;
                tracks.push_back(new_track);
            }
        }

        // 3. Prune tracks
        auto it = tracks.begin();
        while (it != tracks.end()) {
            if (it->missing_frames > 30) { // Lost for ~1 sec
                if (it->id == locked_id) {
                    locked_id = -1; // Unlock
                }
                it = tracks.erase(it);
            } else {
                ++it;
            }
        }

        // 4. Lock logic
        if (locked_id == -1) {
            for (const auto& track : tracks) {
                if (track.consecutive_frames >= 8) {
                    locked_id = track.id;
                    break; 
                }
            }
        }
    }

    std::vector<HumanPoseData> getResult() {
        std::vector<HumanPoseData> result;
        if (locked_id != -1) {
            for (const auto& track : tracks) {
                if (track.id == locked_id) {
                    // Return data if currently detected or missing for short time (smoothing)
                    if (track.missing_frames < 5) {
                        result.push_back(track.data);
                    }
                    return result;
                }
            }
        }
        return result;
    }
    
    int getLockedId() const { return locked_id; }
    
    // Debug info
    void drawDebug(cv::Mat& frame) {
        for (const auto& track : tracks) {
            cv::Scalar color = (track.id == locked_id) ? cv::Scalar(0, 255, 0) : cv::Scalar(0, 255, 255);
            if (track.missing_frames > 0) color = cv::Scalar(100, 100, 100);
            
            cv::circle(frame, track.center, 5, color, 2);
            std::string text = "ID:" + std::to_string(track.id) + " C:" + std::to_string(track.consecutive_frames);
            cv::putText(frame, text, track.center + cv::Point2f(10, 10), cv::FONT_HERSHEY_SIMPLEX, 0.5, color, 1);
        }
    }

private:
    std::vector<TrackedHuman> tracks;
    int next_id;
    int locked_id;

    cv::Point2f getCenter(const HumanPoseData& h) {
        int count = 0;
        cv::Point2f sum(0, 0);
        if (h.right_shoulder[0] != -1) {
            sum += cv::Point2f(h.right_shoulder[0], h.right_shoulder[1]);
            count++;
        }
        if (h.left_shoulder[0] != -1) {
            sum += cv::Point2f(h.left_shoulder[0], h.left_shoulder[1]);
            count++;
        }
        if (count == 0) return cv::Point2f(0, 0);
        return sum * (1.0 / count);
    }
};

#endif
