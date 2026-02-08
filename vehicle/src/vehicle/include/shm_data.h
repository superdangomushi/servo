#ifndef SHM_DATA_H
#define SHM_DATA_H

struct ArUcoMarkerData {
    int id;
    double tvec[3];  // 平行移動ベクトル [x, y, z]
    double rvec[3];  // 回転ベクトル [rx, ry, rz]
    double timestamp; // タイムスタンプ
};

struct HumanPoseData {
    bool detected;
    double left_shoulder[2];  // [x, y] normalized or pixel? Let's use pixel for now or normalized. 
                              // User asked for coordinates. Pixel is usually easier for overlay, but normalized is better for logic.
                              // Let's stick to what OpenCV usually gives or convert to pixel.
    double right_shoulder[2]; // [x, y]
    double timestamp;
};

struct SharedMemoryData {
    // Marker Data
    int marker_count;
    ArUcoMarkerData markers[10]; // 最大10個のマーカー
    double last_marker_update_time;

    // Human Data L
    int human_count_L;
    HumanPoseData humans_L[10]; // 最大10人の人間 (Camera L)
    double last_human_update_time_L;

    // Human Data R
    int human_count_R;
    HumanPoseData humans_R[10]; // 最大10人の人間 (Camera R)
    double last_human_update_time_R;
};

#endif // SHM_DATA_H
