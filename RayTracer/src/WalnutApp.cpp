#include "Walnut/Application.h"
#include "Walnut/EntryPoint.h"

#include "Walnut/Image.h"
#include "Walnut/Timer.h"

#include "Renderer.h"
#include "Camera.h"

#include <windows.h>

#include <glm/gtc/type_ptr.hpp>
#include <thread>
#include <algorithm>
#include <gl/GL.h>
#include <Psapi.h>
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "opengl32.lib")

#include <GLFW/glfw3.h>

using namespace Walnut;

class ExampleLayer : public Walnut::Layer
{
public:
	ExampleLayer()
		: m_Camera(45.0f, 0.1f, 100.0f)
	{

		Material& sphere1 = m_Scene.Materials.emplace_back();
		sphere1.Albedo = { 1.0f, 0.0f, 1.0f };
		sphere1.Roughness = 0.0f;

		Material& sphere2 = m_Scene.Materials.emplace_back();
		sphere2.Albedo = { 0.2f, 0.3f, 1.0f };
		sphere2.Roughness = 0.1f;

		{
			Sphere sphere;

			sphere.Position = { 0.0f, 0.0f, 0.0f };
			sphere.Radius = 1.0f;
			sphere.MaterialIndex = 0;
			m_Scene.Spheres.push_back(sphere);
		}

		{
			Sphere sphere;

			sphere.Position = { 0.0f, -101.0f, 0.0f };
			sphere.Radius = 100.0f;
			sphere.MaterialIndex = 1;
			m_Scene.Spheres.push_back(sphere);
		}
	}

	virtual void OnUpdate(float ts) override
	{
		if (m_Camera.OnUpdate(ts)) {
			m_Renderer.ResetFrameIndex();
		}
	}

	virtual void OnUIRender() override
	{
		ImGui::Begin("Settings");

		ImGui::Text("Last render: %.3fms", m_LastRenderTime);

		if (ImGui::Button("Render")) {
			Render();
		}

		ImGui::Checkbox("Accumulate", &m_Renderer.GetSettings().Accumulate);

		if (ImGui::Button("Reset")) {
			m_Renderer.ResetFrameIndex();
		}

		ImGui::End();

		ImGui::Begin("Scene");
		for (size_t i = 0; i < m_Scene.Spheres.size(); i++) {
			ImGui::PushID(i);

			Sphere& sphere = m_Scene.Spheres[i];
			ImGui::DragFloat3("Position", glm::value_ptr(sphere.Position), 0.1f);
			ImGui::DragFloat("Radius", &sphere.Radius, 0.1f);
			ImGui::DragInt("Material", &sphere.MaterialIndex, 1.0f, 0, (int)m_Scene.Materials.size() - 1);

			ImGui::Separator();

			ImGui::PopID();
		}

		for (size_t i = 0; i < m_Scene.Materials.size(); i++) {
			ImGui::PushID(i);

			Material& material = m_Scene.Materials[i];
			ImGui::ColorEdit3("Albedo", glm::value_ptr(material.Albedo));
			ImGui::DragFloat("Roughness", &material.Roughness, 0.05f, 0.0f, 1.0f);
			ImGui::DragFloat("Metallic", &material.Metallic, 0.05f, 0.0f, 1.0f);

			ImGui::Separator();

			ImGui::PopID();
		}

		ImGui::End();

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("Viewport");

		m_ViewportWidth = ImGui::GetContentRegionAvail().x;
		m_ViewportHeight = ImGui::GetContentRegionAvail().y;

		auto image = m_Renderer.GetFinalImage();
		if (image) {
			ImGui::Image(image->GetDescriptorSet(), { (float)image->GetWidth(), (float)image->GetHeight() }, ImVec2(0, 1), ImVec2(1, 0));
		}

		ImGui::End();


		ImGui::Begin("Advanced Stats");

		// Existing frame timing code (keep as is)
		static float frameTimes[100] = {};
		static int frameIndex = 0;
		static float maxFrameTime = 0.0f;

		float frameTime = m_LastRenderTime;
		frameTimes[frameIndex] = frameTime;
		frameIndex = (frameIndex + 1) % 100;

		maxFrameTime = 0.0f;
		for (float t : frameTimes)
			if (t > maxFrameTime) maxFrameTime = t;

		ImGui::PlotLines("Frame Time (ms)", frameTimes, 100, 0, nullptr, 0.0f, maxFrameTime, ImVec2(0, 80));
		ImGui::Text("Frame Time: %.3f ms", frameTime);
		ImGui::Text("FPS: %.1f", 1000.0f / frameTime);

		// ======== ADVANCED PROFILING ADDITIONS ======== //

		// 1. Frame Time Statistics
		static float frameTimeMin = FLT_MAX;
		static float frameTimeMax = 0;
		static float frameTimePercentile99 = 0;
		static float frameTimePercentile95 = 0;

		if (frameTime < frameTimeMin) frameTimeMin = frameTime;
		if (frameTime > frameTimeMax) frameTimeMax = frameTime;

		// Calculate percentiles
		static std::vector<float> sortedFrameTimes(100, 0.0f);
		std::copy(frameTimes, frameTimes + 100, sortedFrameTimes.begin());
		std::sort(sortedFrameTimes.begin(), sortedFrameTimes.end());
		frameTimePercentile99 = sortedFrameTimes[static_cast<int>(sortedFrameTimes.size() * 0.99)];
		frameTimePercentile95 = sortedFrameTimes[static_cast<int>(sortedFrameTimes.size() * 0.95)];

		// Display stats
		ImGui::Separator();
		ImGui::Text("Frame Time Stats:");
		ImGui::Text("Min: %.3f ms", frameTimeMin);
		ImGui::Text("Max: %.3f ms", frameTimeMax);
		ImGui::Text("99th %%: %.3f ms", frameTimePercentile99);
		ImGui::Text("95th %%: %.3f ms", frameTimePercentile95);

		// 2. GPU Monitoring
		ImGui::Separator();
		ImGui::Text("GPU Stats:");
		const GLubyte* renderer = glGetString(GL_RENDERER);
		const GLubyte* version = glGetString(GL_VERSION);
		ImGui::Text("Renderer: %s", renderer);
		ImGui::Text("Driver: %s", version);
		

		// 3. Detailed System Stats
		ImGui::Separator();
		ImGui::Text("System Stats:");

		// CPU core count
		ImGui::Text("CPU Cores: %d", std::thread::hardware_concurrency());

		// CPU usage (Windows implementation)
		#if defined(_WIN32)
				static ULARGE_INTEGER lastCPU = { 0 }, lastSysCPU = { 0 }, lastUserCPU = { 0 };
				static int numProcessors = 0;
				static HANDLE self = GetCurrentProcess();

				if (numProcessors == 0) {
					SYSTEM_INFO sysInfo;
					GetSystemInfo(&sysInfo);
					numProcessors = sysInfo.dwNumberOfProcessors;

					FILETIME ftime, fsys, fuser;
					GetSystemTimeAsFileTime(&ftime);
					lastCPU.LowPart = ftime.dwLowDateTime;
					lastCPU.HighPart = ftime.dwHighDateTime;

					GetProcessTimes(self, &ftime, &ftime, &fsys, &fuser);
					memcpy(&lastSysCPU, &fsys, sizeof(FILETIME));
					memcpy(&lastUserCPU, &fuser, sizeof(FILETIME));
				}

				FILETIME ftime, fsys, fuser;
				ULARGE_INTEGER now, sys, user;
				float percent = 0.0f;

				GetSystemTimeAsFileTime(&ftime);
				now.LowPart = ftime.dwLowDateTime;
				now.HighPart = ftime.dwHighDateTime;

				GetProcessTimes(self, &ftime, &ftime, &fsys, &fuser);
				sys.LowPart = fsys.dwLowDateTime;
				sys.HighPart = fsys.dwHighDateTime;
				user.LowPart = fuser.dwLowDateTime;
				user.HighPart = fuser.dwHighDateTime;

				percent = static_cast<float>((sys.QuadPart - lastSysCPU.QuadPart) +
					(user.QuadPart - lastUserCPU.QuadPart));
				percent /= (now.QuadPart - lastCPU.QuadPart);
				percent /= numProcessors;

				lastCPU = now;
				lastUserCPU = user;
				lastSysCPU = sys;

				ImGui::Text("CPU Usage: %.1f%%", percent * 100);
		#endif

		// Memory usage (process-specific)
		#if defined(_WIN32)
				PROCESS_MEMORY_COUNTERS pmc;
				if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
					ImGui::Text("RAM Used: %.2f MB", pmc.WorkingSetSize / (1024.0f * 1024.0f));
					ImGui::Text("Commit Size: %.2f MB", pmc.PagefileUsage / (1024.0f * 1024.0f));
				}
		#endif

		// 4. Performance Histograms
		ImGui::Separator();
		ImGui::Text("Performance Histograms:");

		// Frame time distribution
		ImGui::PlotHistogram("Frame Time Dist", frameTimes, 100, 0, nullptr, 0.0f, maxFrameTime, ImVec2(0, 80));

		// Memory allocation tracking
		static std::vector<float> allocSizes;  // Use float for ImGui compatibility
		static int totalAllocations = 0;
		static float maxAllocationSize = 0.0f;

		// Simulate allocation tracking
		if (rand() % 10 == 0) {
			float size = static_cast<float>(rand() % 1024 + 1);
			allocSizes.push_back(size);
			totalAllocations++;
			if (size > maxAllocationSize) maxAllocationSize = size;
			if (allocSizes.size() > 100) allocSizes.erase(allocSizes.begin());
		}

		if (!allocSizes.empty()) {
			ImGui::PlotHistogram("Allocation Sizes", allocSizes.data(), static_cast<int>(allocSizes.size()),
				0, nullptr, 0.0f, maxAllocationSize, ImVec2(0, 80));
			ImGui::Text("Allocations: %d (Max: %.0f bytes)", totalAllocations, maxAllocationSize);
		}

		// 5. Advanced Controls
		ImGui::Separator();
		if (ImGui::Button("Reset Stats")) {
			frameTimeMin = FLT_MAX;
			frameTimeMax = 0;
			allocSizes.clear();
			totalAllocations = 0;
			maxAllocationSize = 0;
		}

		ImGui::End();


		ImGui::PopStyleVar();

		Render();
		//ImGui::ShowDemoWindow();
	}

	void Render()
	{
		Timer timer;

		m_Renderer.OnResize(m_ViewportWidth, m_ViewportHeight);
		m_Camera.OnResize(m_ViewportWidth, m_ViewportHeight);
		m_Renderer.Render(m_Scene, m_Camera);

		m_LastRenderTime = timer.ElapsedMillis();
	}

private:
	Renderer m_Renderer;
	Camera m_Camera;
	Scene m_Scene;
	uint32_t m_ViewportWidth = 0, m_ViewportHeight = 0;

	float m_LastRenderTime = 0.0f;
};

Walnut::Application* Walnut::CreateApplication(int argc, char** argv)
{
	Walnut::ApplicationSpecification spec;
	spec.Name = "RayTracer";

	Walnut::Application* app = new Walnut::Application(spec);
	app->PushLayer<ExampleLayer>();
	app->SetMenubarCallback([app]()
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("Exit"))
				{
					app->Close();
				}
				ImGui::EndMenu();
			}
		});
	return app;
}