#include "CameraSeeThroughComponent.h"
#include <Modules/ModuleManager.h>
#include <array>

UCameraSeeThroughComponent::UCameraSeeThroughComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.SetTickFunctionEnable(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneAsset(TEXT("StaticMesh'/Engine/BasicShapes/Plane.Plane'"));
	
	if (!(PlaneMeshAsset = PlaneAsset.Succeeded() ? PlaneAsset.Object : nullptr))
	{
		UE_LOG(LOG_UNREAL_DR, Error, TEXT("[UnrealDR] Unable to find simple plane static mesh asset."));
		throw;
	}
}

void UCameraSeeThroughComponent::OnRegister()
{
	// Request the VR system from the module instance.
	auto& m = FModuleManager::GetModuleChecked<FUnrealDRModule>("UnrealDR");
	m_system = m.getVirtualRealitySystem();

	if (m_system == nullptr)
	{
		UE_LOG(LOG_UNREAL_DR, Error, TEXT("[UnrealDR] VR System is not initialized. Make sure the VR HMD is turned on and SteamVR has been launched."));
		return;
	}

	// Get a tracked camera interface.
	m_camera = vr::VRTrackedCamera();

	if (m_camera == nullptr)
	{
		UE_LOG(LOG_UNREAL_DR, Error, TEXT("[UnrealDR] Unable to retrieve HMD camera interface."));
		return;
	}

	// Check if a camera is available.
	bool hasCamera{ false };
	auto cameraError = m_camera->HasCamera(vr::k_unTrackedDeviceIndex_Hmd, &hasCamera);

	if (cameraError != vr::EVRTrackedCameraError::VRTrackedCameraError_None)
	{
		UE_LOG(LOG_UNREAL_DR, Error, TEXT("[UnrealDR] Unable to retrieve camera: %d."), cameraError);
		return;
	}
	else if (!hasCamera)
	{
		UE_LOG(LOG_UNREAL_DR, Warning, TEXT("[UnrealDR] The HMD does not provide camera access."));
		return;
	}

	// Allocate for camera frame buffer requirements
	uint32_t frameBufferSize = 0;

	if (m_camera->GetCameraFrameSize(vr::k_unTrackedDeviceIndex_Hmd, vr::VRTrackedCameraFrameType_Undistorted, &m_frameWidth, &m_frameHeight, &frameBufferSize) != vr::VRTrackedCameraError_None)
	{
		UE_LOG(LOG_UNREAL_DR, Error, TEXT("[UnrealDR] Unable to request front camera frame buffer bounds."));
	}
	else
	{
		// Allocate new frame buffer, if size changes.
		if (frameBufferSize != m_frameBufferSize)
		{
			if (m_frameBuffer)
				std::free(m_frameBuffer);

			if (m_frameBufferRegions.Contains(vr::Eye_Left))
				FMemory::Free(m_frameBufferRegions[vr::Eye_Left]);
			else
				m_frameBufferRegions.Add(vr::Eye_Left);

			if (m_frameBufferRegions.Contains(vr::Eye_Right))
				FMemory::Free(m_frameBufferRegions[vr::Eye_Right]);
			else
				m_frameBufferRegions.Add(vr::Eye_Right);

			m_frameBuffer = reinterpret_cast<uint8_t*>(std::calloc(m_frameBufferSize = frameBufferSize, sizeof(uint8_t)));
			m_frameBufferRegions[vr::Eye_Left] = new FUpdateTextureRegion2D(0, 0, 0, m_frameHeight / 2, m_frameWidth, m_frameHeight / 2);
			m_frameBufferRegions[vr::Eye_Right] = new FUpdateTextureRegion2D(0, 0, 0, 0, m_frameWidth, m_frameHeight / 2);

			// Create left and right eye texture instances.
			LeftEyeImage = UTexture2D::CreateTransient(m_frameWidth, m_frameHeight / 2, EPixelFormat::PF_R8G8B8A8, "Left Eye Image");
			LeftEyeImage->AddToRoot();
			LeftEyeImage->UpdateResource();

			RightEyeImage = UTexture2D::CreateTransient(m_frameWidth, m_frameHeight / 2, EPixelFormat::PF_R8G8B8A8, "Right Eye Image");
			RightEyeImage->AddToRoot();
			RightEyeImage->UpdateResource();
		}
	}

	// Create a material instance.
	CameraImageMaterialInstance = UMaterialInstanceDynamic::Create(CameraImageMaterial, this, "Camera Image Material Instance");

	// Request camera extrinsics.
	vr::ETrackedPropertyError extrinsicsError;
	std::array<vr::HmdMatrix34_t, 2> transformBuffer{ vr::HmdMatrix34_t{}, vr::HmdMatrix34_t{} };

	if (m_system->GetArrayTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_CameraToHeadTransforms_Matrix34_Array, vr::k_unHmdMatrix34PropertyTag, transformBuffer.data(), transformBuffer.size() * sizeof(vr::HmdMatrix34_t), &extrinsicsError) == 0 || extrinsicsError != vr::ETrackedPropertyError::TrackedProp_Success)
	{
		UE_LOG(LOG_UNREAL_DR, Warning, TEXT("[UnrealDR] Unable to request camera extrinsic calibration matrices: %d."), extrinsicsError);
	}
	else
	{
		// Create view for each eye.
		for (int e = vr::Eye_Left; e <= vr::Eye_Right; ++e)
		{
			auto eye = reinterpret_cast<vr::EVREye&>(e);

			// Create view planes.
			if (!m_viewPlanes.Contains(eye))
			{
				// Request camera intristics.
				vr::HmdVector2_t focalLength, center;

				auto intrinsicsError = m_camera->GetCameraIntrinsics(vr::k_unTrackedDeviceIndex_Hmd, eye, vr::VRTrackedCameraFrameType_Undistorted, &focalLength, &center);

				if (intrinsicsError != vr::EVRTrackedCameraError::VRTrackedCameraError_None)
				{
					UE_LOG(LOG_UNREAL_DR, Error, TEXT("[UnrealDR] Unable to get intristics for camera %d: %d."), e, intrinsicsError);
				}
				else
				{
					auto plane = this->CreateViewPlaneMesh(eye == vr::Eye_Left ? TEXT("Left Eye View Plane") : TEXT("Right Eye View Plane"), this->GetEyeAnchor(eye), FVector2D(center.v[0], center.v[1]), FVector2D(focalLength.v[0], focalLength.v[1]));

					if (plane == nullptr)
					{
						UE_LOG(LOG_UNREAL_DR, Error, TEXT("[UnrealDR] Unable to create view plane mesh for eye %d."), e);
					}
					else
					{
						// Assign the material to the viewplane.
						plane->SetMaterial(0, CameraImageMaterialInstance);

						// Store the view plane.
						m_viewPlanes.Add(eye) = plane;
					}
				}
			}

#if WITH_EDITORONLY_DATA
			// Register draw frustum for editor preview.
			if (!this->EditorDrawFrustums.Contains(eye))
			{
				auto frustum = NewObject<UDrawFrustumComponent>(this, NAME_None, RF_Transactional);
				frustum->SetupAttachment(this);
				frustum->SetIsVisualizationComponent(true);
				frustum->CreationMethod = this->CreationMethod;
				frustum->RegisterComponentWithWorld(this->GetWorld());

				FVector translation;
				FRotator rotation;
				this->GetCameraPoseFromExtrinsicTransform(transformBuffer[e], translation, rotation);

				frustum->AddLocalOffset(translation);
				frustum->AddLocalRotation(rotation);

				// Create resources.
				if (m_camera)
				{
					// TODO: Use intristics to compute FoV
					frustum->FrustumColor = eye == vr::Eye_Left ? FColor::Blue : FColor::Green;
					frustum->FrustumAngle = 90.0f;
					frustum->FrustumAspectRatio = 16.f / 9.f;
					frustum->FrustumStartDist = 1.f;
					frustum->FrustumEndDist = 100.f;
				}
				else
				{
					UE_LOG(LOG_UNREAL_DR, Warning, TEXT("[UnrealDR] The OpenVR camera instance is could not be initialized."));

					frustum->FrustumColor = FColor::Red;
					frustum->FrustumAngle = 90.0f;
					frustum->FrustumAspectRatio = 16.f / 9.f;
					frustum->FrustumStartDist = 1.f;
					frustum->FrustumEndDist = 100.f;
				}

				frustum->MarkRenderStateDirty();
				this->EditorDrawFrustums.Add(eye) = frustum;
			}
#endif
		}
	}

	// Request World-To-Meters scale.
	m_worldToMeters = this->GetWorld()->GetWorldSettings()->WorldToMeters;

	Super::OnRegister();
}

void UCameraSeeThroughComponent::OnUnregister()
{
	m_system = nullptr;

#if WITH_EDITORONLY_DATA
	if (this->EditorDrawFrustums.Contains(vr::Eye_Left))
		this->EditorDrawFrustums[vr::Eye_Left]->DestroyComponent();

	if (this->EditorDrawFrustums.Contains(vr::Eye_Right))
		this->EditorDrawFrustums[vr::Eye_Right]->DestroyComponent();

	this->EditorDrawFrustums.Empty();
#endif // WITH_EDITORONLY_DATA

	if (this->m_viewPlanes.Contains(vr::Eye_Left))
		this->m_viewPlanes[vr::Eye_Left]->DestroyComponent();

	if (this->m_viewPlanes.Contains(vr::Eye_Right))
		this->m_viewPlanes[vr::Eye_Right]->DestroyComponent();

	this->m_viewPlanes.Empty();

	Super::OnUnregister();
}

void UCameraSeeThroughComponent::BeginPlay()
{
	// Receive a tracked camera handle.
	m_trackedCamera = m_camera == nullptr ? INVALID_TRACKED_CAMERA_HANDLE : this->StartStreaming(m_camera);

	// Create resources.
	if (m_trackedCamera == INVALID_TRACKED_CAMERA_HANDLE)
	{
		UE_LOG(LOG_UNREAL_DR, Error, TEXT("[UnrealDR] Unable to start camera streaming."));
	}
	else
	{
		// Mark component BeginPlay as routed.
		Super::BeginPlay();
	}
}

void UCameraSeeThroughComponent::EndPlay(const EEndPlayReason::Type reason)
{
	/*
	// TODO: Check how to properly explicitly delete objects.
	if (CameraImageMaterialInstance)
		delete CameraImageMaterialInstance;

	CameraImageMaterialInstance = nullptr;

	if (LeftEyeImage)
		delete LeftEyeImage;
	
	LeftEyeImage = nullptr;

	if (RightEyeImage)
		delete RightEyeImage;

	RightEyeImage = nullptr;
	*/

	if (m_viewPlanes.Contains(vr::Eye_Left))
		m_viewPlanes[vr::Eye_Left]->DestroyComponent();

	if (m_viewPlanes.Contains(vr::Eye_Right))
		m_viewPlanes[vr::Eye_Right]->DestroyComponent();

	if (m_camera)
		this->StopStreaming(m_camera, m_trackedCamera);

	m_camera = nullptr;

	// Mark component EndPlay as routed.
	Super::EndPlay(reason);
}

vr::TrackedCameraHandle_t UCameraSeeThroughComponent::StartStreaming(vr::IVRTrackedCamera* camera)
{
	if (camera == nullptr)
		return INVALID_TRACKED_CAMERA_HANDLE;

	// Reset video stream.
	m_lastFrameIndex = 0;
	m_timeSinceLastFrameUpdate = 0;

	// Get a tracked camera handle.
	vr::TrackedCameraHandle_t cameraHandle{ INVALID_TRACKED_CAMERA_HANDLE };
	camera->AcquireVideoStreamingService(vr::k_unTrackedDeviceIndex_Hmd, &cameraHandle);

	return cameraHandle;
}

void UCameraSeeThroughComponent::StopStreaming(vr::IVRTrackedCamera* camera, vr::TrackedCameraHandle_t& trackedCamera) const
{
	camera->ReleaseVideoStreamingService(trackedCamera);
	trackedCamera = INVALID_TRACKED_CAMERA_HANDLE;
}

void UCameraSeeThroughComponent::UpdateImages()
{
	// TODO: This can be improved by directly accessing the frame buffer as a texture, so no roudtrip to CPU memory is required.
	LeftEyeImage->UpdateTextureRegions(0, 1, m_frameBufferRegions[vr::Eye_Left], static_cast<uint32_t>(m_frameWidth * sizeof(uint8_t) * 4), sizeof(uint8_t) * 4, m_frameBuffer, [this](auto rawData, auto region) { this->CleanupFrameBufferRegion(rawData, region); });
	RightEyeImage->UpdateTextureRegions(0, 1, m_frameBufferRegions[vr::Eye_Right], static_cast<uint32_t>(m_frameWidth * sizeof(uint8_t) * 4), sizeof(uint8_t) * 4, m_frameBuffer, [this](auto rawData, auto region) { this->CleanupFrameBufferRegion(rawData, region); });
}

void UCameraSeeThroughComponent::CleanupFrameBufferRegion(uint8_t* rawData, const FUpdateTextureRegion2D* region) const noexcept
{
	// No need to clean-up.
}

UStaticMeshComponent* UCameraSeeThroughComponent::CreateViewPlaneMesh(FName name, const FVector v, const FVector2D center, const FVector2D focalLength) noexcept
{
	UStaticMeshComponent* component = NewObject<UStaticMeshComponent>(this, name);

	if (!component) 
		return nullptr;
	
	component->SetupAttachment(this);
	component->SetStaticMesh(this->PlaneMeshAsset);
	component->RegisterComponent();
	component->SetHiddenInGame(false);
	component->SetRelativeLocation(v);
	component->SetRelativeRotation(FRotator(0.0f, 90.0f, 90.0f));
	component->SetRelativeScale3D(FVector(10.f * static_cast<float>(m_frameWidth) / focalLength.X, 10.f * (static_cast<float>(m_frameHeight) / 2.f) / focalLength.Y, 1.0f));
	component->SetCollisionProfileName("NoCollision");
	component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	component->bCastDynamicShadow = false;
	component->CastShadow = false;

	component->SetIsVisualizationComponent(true);
	component->CreationMethod = this->CreationMethod;
	component->RegisterComponentWithWorld(this->GetWorld());

	return component;
}

void UCameraSeeThroughComponent::TickComponent(float deltaTime, ELevelTick tickType, FActorComponentTickFunction* f)
{
	if (tickType == ELevelTick::LEVELTICK_PauseTick)
		return;

	// Only handle valid camera modules.
	if (m_trackedCamera != INVALID_TRACKED_CAMERA_HANDLE)
	{
		// Check frame buffer header for updates.
		vr::CameraVideoStreamFrameHeader_t frameHeader;
		vr::EVRTrackedCameraError error = m_camera->GetVideoStreamFrameBuffer(m_trackedCamera, vr::VRTrackedCameraFrameType_Undistorted, nullptr, 0, &frameHeader, sizeof(frameHeader));

		if (error != vr::VRTrackedCameraError_None)
		{
			UE_LOG(LOG_UNREAL_DR, Error, TEXT("[UnrealDR] Unable to access front camera video stream frame buffer: %d."), error);
		}
		else
		{
			if (frameHeader.nFrameSequence == m_lastFrameIndex)
			{
				m_timeSinceLastFrameUpdate += deltaTime;

				if (m_timeSinceLastFrameUpdate > 2000.f)
				{
					UE_LOG(LOG_UNREAL_DR, Warning, TEXT("[UnrealDR] No frames arriving."));
				}
			}
			else
			{
				// Reset time since last update.
				m_timeSinceLastFrameUpdate = 0.f;

				// Copy frame buffer.
				error = m_camera->GetVideoStreamFrameBuffer(m_trackedCamera, vr::VRTrackedCameraFrameType_Undistorted, m_frameBuffer, m_frameBufferSize, &frameHeader, sizeof(frameHeader));
				
				if (error != vr::VRTrackedCameraError_None)
				{
					UE_LOG(LOG_UNREAL_DR, Error, TEXT("[UnrealDR] Unable to copy front camera video stream frame buffer."), error);
				}
				else
				{
					// Push the frame buffer into the left/right eye textures.
					this->UpdateImages();

					// Update material instance.
					CameraImageMaterialInstance->SetTextureParameterValue(TEXT("LeftEye"), LeftEyeImage);
					CameraImageMaterialInstance->SetTextureParameterValue(TEXT("RightEye"), RightEyeImage);

					// Store current frame as last frame.
					m_lastFrameIndex = frameHeader.nFrameSequence;
				}
			}
		}
	}

	Super::TickComponent(deltaTime, tickType, f);
}