#include "UnrealDR.h"

#define LOCTEXT_NAMESPACE "FUnrealDRModule"

void FUnrealDRModule::StartupModule()
{
	// Check if the HMD is available.
	if (!vr::VR_IsHmdPresent())
	{
		UE_LOG(LOG_UNREAL_DR, Error, TEXT("[UnrealDR] No virtual reality HMD has been detected."));
		return;
	}

	// Check if the SteamVR runtime is installed.
	if (!vr::VR_IsRuntimeInstalled())
	{
		UE_LOG(LOG_UNREAL_DR, Error, TEXT("[UnrealDR] SteamVR runtime is not installed."));
		return;
	}

	// Create a VR system.
	vr::EVRInitError error{ vr::EVRInitError::VRInitError_None };
	m_system = vr::VR_Init(&error, vr::VRApplication_Background);

	if (error != vr::EVRInitError::VRInitError_None) 
	{
		UE_LOG(LOG_UNREAL_DR, Error, TEXT("[UnrealDR] Unable to initialize VR system: %d."), error);
		return;
	}
}

void FUnrealDRModule::ShutdownModule()
{
	// Unload OpenVR.
	if (m_system != nullptr)
		vr::VR_Shutdown();

	m_system = nullptr;
}

vr::IVRSystem* FUnrealDRModule::getVirtualRealitySystem() const
{
	return m_system;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FUnrealDRModule, UnrealDR)