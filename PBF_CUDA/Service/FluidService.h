#pragma once

#include <memory>
#include <mutex>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "../Simulator/FluidSystem.h"
#include "Json.h"

// 一个简单的服务类，管理 FluidSystem 的生命周期与数据 I/O
// 协议：所有请求为 JSON 对象，以整数字段 `type` 作为操作码
// 类型一览：
// 1 reset { type:1, gridSize:64, radius:0.1, numParticles:100000 } // 重置系统并指定最大容量
// 2 addFluidPoints { type:2, fluid:{ positions:[[x,y,z],...], velocities:[[vx,vy,vz],...] } } // 添加流体粒子点云，速度可选，默认 0
// 4 step { type:4, dt:0.016, solids:[{id, rotation:[w,x,y,z], translation:[x,y,z]}] } // 前进一步，可选更新固体变换，返回状态
// 5 clear { type:5 } // 清空流体与固体
// 6 getState { type:6 } // 返回 positions, velocities（展平的 XYZ 数组）
// 7 upsertSolidPointCloud { type:7, solid:{ id:string, positions:[[x,y,z],...], normals:[[x,y,z],...] } } // 新增或替换固体点云
// 返回对象：{ ok:true/false, msg:string, ...payload }
class FluidService {
public:
	FluidService() = default;

	std::string handle(const std::string &json);

private:
	// helpers
	bool ensureSystem(unsigned int capacity=0, unsigned int grid=64, float radius=0.1f);

	MiniJson::Value okResult(){ MiniJson::Value v=MiniJson::Value::object(); v.o["ok"]=MiniJson::Value::boolean(true); return v; }
	MiniJson::Value errResult(const std::string &m){ MiniJson::Value v=MiniJson::Value::object(); v.o["ok"]=MiniJson::Value::boolean(false); v.o["msg"]=MiniJson::Value::string(m); return v; }

	struct SolidPointCloud {
		std::vector<float> positions; // [x,y,z]*B
		std::vector<float> normals;   // [x,y,z]*B
		glm::quat rotation{1,0,0,0};
		glm::vec3 translation{0,0,0};
	};

private:
	std::mutex _mtx;
	Simulator::FluidSystem::ptr _system; // 服务使用 headless 模式
	unsigned int _gridSize = 64;
	float _radius = 0.1f;
	unsigned int _capacity = 0; // 最大容量

	// 以 id 索引的固体点云
	std::unordered_map<std::string, SolidPointCloud> _solids;
};
