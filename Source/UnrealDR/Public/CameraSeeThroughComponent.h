#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "Components/SceneComponent.h"

#if WITH_EDITORONLY_DATA
#include "Components/DrawFrustumComponent.h"
#endif // WITH_EDITORONLY_DATA

#include "UnrealDR.h"
#include "CameraSeeThroughComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UNREALDR_API UCameraSeeThroughComponent : public USceneComponent
{
	GENERATED_BODY()

private:
	/// <summary>
	/// Interface pointer to the VR system instance.
	/// </summary>
	vr::IVRSystem* m_system{ nullptr };

	/// <summary>
	/// Interface pointer to the tracked camera instance.
	/// </summary>
	vr::IVRTrackedCamera* m_camera{ nullptr };

	/// <summary>
	/// Handle of the tracked camera.
	/// </summary>
	vr::TrackedCameraHandle_t m_trackedCamera{ INVALID_TRACKED_CAMERA_HANDLE };

	/// <summary>
	/// Camera stream frame buffer width.
	/// </summary>
	uint32_t m_frameWidth{ 0 };

	/// <summary>
	/// Camera stream frame buffer height.
	/// </summary>
	uint32_t m_frameHeight{ 0 };
	
	/// <summary>
	/// Camera stream frame buffer size in bytes.
	/// </summary>
	uint32_t m_frameBufferSize{ 0 };
	
	/// <summary>
	/// Index of last streamed image.
	/// </summary>
	uint32_t m_lastFrameIndex{ 0 };
	
	/// <summary>
	/// Pointer to the camera stream frame buffer.
	/// </summary>
	uint8_t* m_frameBuffer{ nullptr };

	/// <summary>
	/// The regions of the frame images to update from the frame buffer. One for each eye.
	/// </summary>
	TMap<vr::EVREye, FUpdateTextureRegion2D*> m_frameBufferRegions;

	/// <summary>
	/// Time since last valid image update from camera stream.
	/// </summary>
	float m_timeSinceLastFrameUpdate{ 0 };

	/// <summary>
	/// The planes that receive the camera images for each eye.
	/// </summary>
	TMap<vr::EVREye, UStaticMeshComponent*> m_viewPlanes;

	/// <summary>
	/// The unit scale from world coordinates to actual meters.
	/// </summary>
	float m_worldToMeters = 100.f;

	// Components that are only visible in the editor.
#if WITH_EDITORONLY_DATA
private:
	/// <summary>
	/// The frustum to display in the editor.
	/// </summary>
	TMap<vr::EVREye, UDrawFrustumComponent*> EditorDrawFrustums;
#endif //WITH_EDITORONLY_DATA

private:
	UStaticMesh* PlaneMeshAsset;

	// Properties
public:
	/// <summary>
	/// The material used to render camera stream images.
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Meta = (ExposeOnSpawn = true), Category = "UnrealDR|Camera Stream") UMaterialInterface* CameraImageMaterial { nullptr };

	/// <summary>
	/// The instance of a material used to render camera images.
	/// </summary>
	/// <seealso cref="CameraImageMaterial" />
	UPROPERTY(BlueprintReadOnly, Category = "UnrealDR|Camera Stream") UMaterialInstanceDynamic* CameraImageMaterialInstance { nullptr };

	/// <summary>
	/// The image captured for the left eye.
	/// </summary>
	UPROPERTY(BlueprintReadOnly, Category = "UnrealDR|Camera Stream") UTexture2D* LeftEyeImage { nullptr };

	/// <summary>
	/// The image captured for the right eye.
	/// </summary>
	UPROPERTY(BlueprintReadOnly, Category = "UnrealDR|Camera Stream") UTexture2D* RightEyeImage { nullptr };

	/// <summary>
	/// Anchor position for the left eye.
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Meta = (ExposeOnSpawn = true), Category = "UnrealDR|Camera Settings") FVector LeftEyeAnchor = FVector((10.f - 0.071f), -0.0325f, 0.0026f);

	/// <summary>
	/// Anchor position for the right eye.
	/// </summary>
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Meta = (ExposeOnSpawn = true), Category = "UnrealDR|Camera Settings") FVector RightEyeAnchor = FVector((10.f - 0.071f), 0.0325f, 0.0026f);
	
public:	
	UCameraSeeThroughComponent();

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type reason) override;

	vr::TrackedCameraHandle_t StartStreaming(vr::IVRTrackedCamera* camera);
	void StopStreaming(vr::IVRTrackedCamera* camera, vr::TrackedCameraHandle_t& trackedCamera) const;
	virtual void UpdateImages();
	virtual void CleanupFrameBufferRegion(uint8_t* rawData, const FUpdateTextureRegion2D* region) const noexcept;
	virtual UStaticMeshComponent* CreateViewPlaneMesh(FName name, const FVector view, const FVector2D center, const FVector2D focalLength) noexcept;

protected:
	static const FTransform& OpenVRToUnrealEngine() noexcept
	{
		// OpenVR uses a right-handed coordinate system, where the distance is measured in meters.
		//		Up: +y, Right: +x, Forward: -z.
		// Unreal Engine uses a left-handed coordinate system, where the distance is measured in "Unreal Units" (UU). One UU typically represents one centimeter. To convert between units, use `m_worldToMeters`.
		//		Up: +z, Right: +y, Forward: +x
		
		// To transform between both coordinate systems, this function provides a constant that can be used. It applies the following transform in order:
		// - Rotate -90° around X axis (roll)
		// - Flip Y axis
		// - Rotate -90° around Z axis (yaw)

		// Here's how the transform can be obtained manually:
		// FRotationMatrix roll(FRotator(0.f, 0.f, -90.f));
		// roll.Mirror(EAxis::None, EAxis::Y);
		// FRotator yaw(0.f, -90.f, 0.f);
		// const FTransform openVrToUnreal = FTransform(roll) * FTransform(yaw);
		
		// This matrix is a constant pre-computed version of the actual transform that needs to be applied in order to transform from OpenVR to Unreal Engine.
		static const FTransform openVrToUnreal(FMatrix(
			FPlane(0.f, -1.f, 0.f, 0.f),
			FPlane(0.f, 0.f, -1.f, 0.f),
			FPlane(-1.f, 0.f, 0.f, 0.f),
			FPlane(0.f, 0.f, 0.f, 1.f)
		));

		return openVrToUnreal;
	}

	inline const FVector GetEyeAnchor(const vr::EVREye& eye) const noexcept
	{
		switch (eye)
		{
		case vr::Eye_Left: return this->LeftEyeAnchor * m_worldToMeters;
		case vr::Eye_Right: return this->RightEyeAnchor * m_worldToMeters;
		default: return FVector(0.f);
		}
	}

	inline void GetCameraPoseFromExtrinsicTransform(const vr::HmdMatrix34_t& cameraToHead, FVector& translation, FRotator& rotation) const noexcept
	{
		auto transform = FTransform(FMatrix(
			FPlane(cameraToHead.m[0][0], cameraToHead.m[0][1], cameraToHead.m[0][2], 0.f),
			FPlane(cameraToHead.m[1][0], cameraToHead.m[1][1], cameraToHead.m[1][2], 0.f),
			FPlane(cameraToHead.m[2][0], cameraToHead.m[2][1], cameraToHead.m[2][2], 0.f),
			FPlane(0.f, 0.f, 0.f, 1.f)).Rotator(),
			FVector(cameraToHead.m[0][3], cameraToHead.m[1][3], cameraToHead.m[2][3]),
			FVector(0.f)
		) * this->OpenVRToUnrealEngine();

		translation = transform.TransformPosition(FVector4(0.f, 0.f, 0.f, 1.f)) * m_worldToMeters;
		rotation = transform.Rotator();
	}

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction = nullptr) override;
};
