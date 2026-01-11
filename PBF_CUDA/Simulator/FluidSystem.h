#pragma once

#include <vector>
#include <memory>
#include <driver_types.h>
#include "SimulateParams.cuh"

namespace Simulator
{
	class FluidSystem
	{
	private:
		bool m_initialized;
		bool m_useGLInterop; // 是否使用 OpenGL 互操作（服务模式下为 false）
		unsigned int m_capacity = 0; // 设备缓冲区容量（以粒子为单位）
		float *m_devicePos;            // position, density.
		float *m_deviceVel;            // velocity, lambda.
		float *m_deviceDeltaPos;
		float *m_devicePredictedPos;
		unsigned int *m_deviceCellStart;
		unsigned int *m_deviceCellEnd;
		unsigned int *m_deviceGridParticleHash;
		
		unsigned int m_posVBO;
		SimulateParams m_params; // m_numParticles 表示当前活动粒子数
		cudaGraphicsResource *m_cudaPosVBORes;

	public:
		typedef std::shared_ptr<FluidSystem> ptr;

		// numParticles 作为容量，初始活动粒子为 0；useGLInterop 控制是否创建 GL 互操作资源
		FluidSystem(unsigned int numParticles, uint3 gridSize, float radius, bool useGLInterop = true);
		~FluidSystem();

		void simulate(float deltaTime);

		unsigned int getPosVBO()const { return m_posVBO; }
		SimulateParams &getSimulateParams() { return m_params; }
		void setResetDensity(const float &value);
		void setParticlePositions(const float *data, int start, int nums);
		void setParticleVelocities(const float *data, int start, int nums);

		void addParticles(const std::vector<float> &pos, const std::vector<float> &vel, unsigned int num);
		void clearParticles() { m_params.m_numParticles = 0; }

		// 服务端需要：从设备下载 XYZ 数据（忽略第 4 分量）
		bool downloadPositionsXYZ(std::vector<float> &outXYZ) const;
		bool downloadVelocitiesXYZ(std::vector<float> &outXYZ) const;

	public:
		void initialize(int numParticles);
		void finalize();
	};
}
