#pragma once

// 背景の共有アセット名と、街区の接続位置。
namespace SceneryAssets {
    inline constexpr float kSceneryNearLocalZ = -24.0f;
    // カメラの後方まで地表を残し、520mの遠クリップより外でだけ背景を循環させる。
    inline constexpr float kSceneryRecycleNearLocalZ = -64.0f;
    inline constexpr float kSceneryFarLocalZ = 540.0f;
    inline constexpr const char* kCityStreet4LaneModelPath =
        "free_models/Downtown City MegaKit[Standard]/"
        "Exports/glTF (Godot)/Street_4Lane.gltf";
    inline constexpr const char* kCityManholeCoverModelPath =
        "free_models/Downtown City MegaKit[Standard]/"
        "Exports/glTF (Godot)/Prop_ManholeCover.gltf";
    inline constexpr const char* kCityDrainModelPath =
        "free_models/Downtown City MegaKit[Standard]/"
        "Exports/glTF (Godot)/Prop_Drain.gltf";
    inline constexpr const char* kCityBuildingSmallModelPath =
        "free_models/Downtown City MegaKit[Standard]/"
        "Exports/glTF (Godot)/Building_Small_1.gltf";
    inline constexpr const char* kCityBuildingMediumModelPath =
        "free_models/Downtown City MegaKit[Standard]/"
        "Exports/glTF (Godot)/Building_Medium_2_001.gltf";
    inline constexpr const char* kCityBuildingLargeModelPath =
        "free_models/Downtown City MegaKit[Standard]/"
        "Exports/glTF (Godot)/Building_Large_2.gltf";
    inline constexpr const char* kTransitLandmarkModelPath = "free_models/transit_landmark/TransitLandmark.gltf";
    inline constexpr const char* kTracksideModelPath = "scenery/FlightTrackside.gltf";
    inline constexpr const char* kAviationDistrictModelPath = "scenery/aviation_district/AviationDistrict.gltf";
    inline constexpr const char* kAviationGlazingModelPath = "scenery/aviation_district/AviationGlazing.gltf";
    inline constexpr const char* kAviationFlightwayModelPath = "scenery/aviation_district/AviationFlightway.gltf";
    inline constexpr const char* kAviationDefenseModelPath = "scenery/aviation_district/AviationDefense.gltf";
    inline constexpr const char* kAviationDefenseGlazingModelPath = "scenery/aviation_district/AviationDefenseGlazing.gltf";
    inline constexpr const char* kAviationDefenseFlightwayModelPath = "scenery/aviation_district/AviationDefenseFlightway.gltf";
    inline constexpr const char* kAviationDefenseRelayModelPath = "scenery/aviation_district/AviationDefenseRelay.gltf";
    inline constexpr const char* kAviationDefenseRelayGlazingModelPath = "scenery/aviation_district/AviationDefenseRelayGlazing.gltf";
    inline constexpr const char* kAviationBossApronModelPath = "scenery/aviation_district/AviationBossApron.gltf";
    inline constexpr const char* kAviationBossApronGlazingModelPath = "scenery/aviation_district/AviationBossApronGlazing.gltf";
    inline constexpr const char* kAviationBossApronSurfaceModelPath = "scenery/aviation_district/AviationBossApronSurface.gltf";
    inline constexpr const char* kAviationBossApronFreightModelPath = "scenery/aviation_district/AviationBossApronFreight.gltf";
    inline constexpr const char* kAviationBossApronFreightGlazingModelPath = "scenery/aviation_district/AviationBossApronFreightGlazing.gltf";
    inline constexpr const char* kAviationTerrainModelPath = "scenery/aviation_district/AviationTerrain.gltf";
    inline constexpr const char* kAviationCampusGroundModelPath = "scenery/aviation_district/AviationCampusGround.gltf";
    inline constexpr const char* kAviationBuildingSupportModelPath = "scenery/aviation_district/AviationBuildingSupport.gltf";
    inline constexpr const char* kAviationBuildingOperationsModelPath = "scenery/aviation_district/AviationBuildingOperations.gltf";
    inline constexpr const char* kAviationBuildingHangarModelPath = "scenery/aviation_district/AviationBuildingHangar.gltf";
    inline constexpr float kDefenseStartWorldZ = 450.0f;
    inline constexpr float kPlazaStartWorldZ = 866.0f;
    inline constexpr float kDefenseModuleLength = 128.0f;
    inline constexpr int kDefenseModuleCount = 6;
    inline constexpr float kDefenseLoopLength = kDefenseModuleLength * kDefenseModuleCount;
    inline constexpr float kBossApronModuleLength = 256.0f;
    inline constexpr int kBossApronModuleCount = 4;
    inline constexpr float kBossApronLoopLength = kBossApronModuleLength * kBossApronModuleCount;
    inline constexpr float kTransitLandmarkWorldZ = 240.0f; // 市街地の途中に固定。撃破速度では位置を変えない。
}
