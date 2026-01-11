#include "FluidService.h"
#include <sstream>

using namespace MiniJson;
using namespace Simulator;

bool FluidService::ensureSystem(unsigned int capacity, unsigned int grid, float radius)
{
	if(_system && _gridSize==grid && _radius==radius && _capacity==capacity) return true;
	_gridSize = grid; _radius = radius; _capacity = capacity;
	uint3 gs{grid,grid,grid};
	_system = std::make_shared<FluidSystem>(capacity, gs, radius, /*useGLInterop*/false);
	return true;
}

static void dumpFlatArray(Value &dst, const std::vector<float> &data){
	dst = Value::array();
	for(float x: data){ Value t; t.type=Value::Number; t.n=x; dst.a.push_back(t);} 
}

std::string FluidService::handle(const std::string &js)
{
	std::lock_guard<std::mutex> lock(_mtx);
	Value root; parse(js, root);
	int type = (int)(root.o.count("type")? root.o["type"].n : 0);
	Value result = okResult();

	switch(type){
		case 1: { // reset
			unsigned int grid = (unsigned int)(root.o.count("gridSize")? root.o["gridSize"].n : _gridSize);
			float rad = (float)(root.o.count("radius")? root.o["radius"].n : _radius);
			unsigned int cap = (unsigned int)(root.o.count("numParticles")? root.o["numParticles"].n : _capacity);
			ensureSystem(cap, grid, rad);
			// 清空已有粒子与固体
			if(_system) _system->clearParticles();
			_solids.clear();
			result.o["msg"]=Value::string("reset ok");
			result.o["config"]=Value::object();
			result.o["config"].o["gridSize"]=Value::number(grid);
			result.o["config"].o["radius"]=Value::number(rad);
			result.o["config"].o["numParticles"]=Value::number(cap);
			break; }
		case 2: { // addFluidPoints
			if(!_system) { result=errResult("not initialized"); break; }
			if(!root.o.count("fluid") || root.o.at("fluid").type!=Value::Object){ result=errResult("fluid missing"); break; }
			const Value &fv = root.o.at("fluid");
			std::vector<float> pos4, vel4;
			// positions [N,3]
			if(fv.o.count("positions") && fv.o.at("positions").type==Value::Array){
				for(const auto &p: fv.o.at("positions").a){
					if(p.type!=Value::Array || p.a.size()<3) continue;
					pos4.push_back((float)p.a[0].n);
					pos4.push_back((float)p.a[1].n);
					pos4.push_back((float)p.a[2].n);
					pos4.push_back(1.0f);
				}
			} else { result=errResult("positions missing"); break; }
			// velocities [N,3] optional
			bool hasVel = fv.o.count("velocities") && fv.o.at("velocities").type==Value::Array;
			if(hasVel){
				for(const auto &v: fv.o.at("velocities").a){
					if(v.type!=Value::Array || v.a.size()<3) continue;
					vel4.push_back((float)v.a[0].n);
					vel4.push_back((float)v.a[1].n);
					vel4.push_back((float)v.a[2].n);
					vel4.push_back(0.0f);
				}
			}
			// 如果未提供速度或数量不匹配，补零
			unsigned n = (unsigned)(pos4.size()/4);
			if(vel4.size()/4 != n){ vel4.assign(n*4, 0.0f); }
			_system->addParticles(pos4, vel4, n);
			result.o["added"]=Value::number((double)n);
			break; }
		case 4: { // step + optional solids transform update, and return state
			if(!_system) { result=errResult("not initialized"); break; }
			// 可选固体变换更新
			if(root.o.count("solids") && root.o["solids"].type==Value::Array){
				for(const auto &item: root.o["solids"].a){
					if(item.type!=Value::Object) continue;
					std::string id = item.o.count("id")? item.o.at("id").s : std::string();
					if(id.empty()) continue;
					auto &pc = _solids[id]; // 若不存在则创建占位
					if(item.o.count("rotation") && item.o.at("rotation").a.size()>=4){
						float w=(float)item.o.at("rotation").a[0].n;
						float x=(float)item.o.at("rotation").a[1].n;
						float y=(float)item.o.at("rotation").a[2].n;
						float z=(float)item.o.at("rotation").a[3].n;
						pc.rotation = glm::quat(w,x,y,z);
					}
					if(item.o.count("translation") && item.o.at("translation").a.size()>=3){
						pc.translation = glm::vec3(
							(float)item.o.at("translation").a[0].n,
							(float)item.o.at("translation").a[1].n,
							(float)item.o.at("translation").a[2].n);
					}
				}
			}
			float dt = (float)(root.o.count("dt")? root.o["dt"].n : 0.016);
			_system->simulate(dt);
			// 返回最新状态
			std::vector<float> p,v; _system->downloadPositionsXYZ(p); _system->downloadVelocitiesXYZ(v);
			Value arrP, arrV; dumpFlatArray(arrP, p); dumpFlatArray(arrV, v);
			result.o["ok"]=Value::boolean(true);
			result.o["positions"]=arrP; result.o["velocities"]=arrV;
			break; }
		case 5: { // clear
			if(!_system) { result=errResult("not initialized"); break; }
			_system->clearParticles();
			_solids.clear();
			result.o["msg"]=Value::string("cleared");
			break; }
		case 6: { // getState
			if(!_system) { result=errResult("not initialized"); break; }
			std::vector<float> p,v; _system->downloadPositionsXYZ(p); _system->downloadVelocitiesXYZ(v);
			Value arrP, arrV; dumpFlatArray(arrP, p); dumpFlatArray(arrV, v);
			result.o["positions"]=arrP; result.o["velocities"]=arrV;
			break; }
		case 7: { // upsertSolidPointCloud
			if(!root.o.count("solid") || root.o.at("solid").type!=Value::Object){ result=errResult("solid missing"); break; }
			const Value &sv = root.o.at("solid");
			if(!sv.o.count("id") || sv.o.at("id").type!=Value::String){ result=errResult("solid.id missing"); break; }
			std::string id = sv.o.at("id").s;
			if(!sv.o.count("positions") || sv.o.at("positions").type!=Value::Array){ result=errResult("positions missing"); break; }
			if(!sv.o.count("normals") || sv.o.at("normals").type!=Value::Array){ result=errResult("normals missing"); break; }
			auto &pc = _solids[id];
			pc.positions.clear(); pc.normals.clear();
			const auto &pa = sv.o.at("positions").a;
			const auto &na = sv.o.at("normals").a;
			if(pa.size()!=na.size()){ result=errResult("positions/normals size mismatch"); break; }
			for(size_t i=0;i<pa.size();++i){
				if(pa[i].type!=Value::Array || pa[i].a.size()<3) continue;
				if(na[i].type!=Value::Array || na[i].a.size()<3) continue;
				pc.positions.push_back((float)pa[i].a[0].n);
				pc.positions.push_back((float)pa[i].a[1].n);
				pc.positions.push_back((float)pa[i].a[2].n);
				pc.normals.push_back((float)na[i].a[0].n);
				pc.normals.push_back((float)na[i].a[1].n);
				pc.normals.push_back((float)na[i].a[2].n);
			}
			result.o["ok"]=Value::boolean(true);
			result.o["id"]=Value::string(id);
			result.o["count"]=Value::number((double)(pc.positions.size()/3));
			break; }
		default:
			result = errResult("unknown type");
	}
	return stringify(result);
}
