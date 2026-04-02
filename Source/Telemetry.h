#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace JPH {
	class BodyInterface;
}

using namespace JPH;

#pragma pack(push, 1)
struct TelemetryHeader {
	uint8_t version = 1;
	uint8_t scenarioId = 0;
	uint16_t reserved = 0;
	uint32_t frameCount = 0;
	float deltaTime = 1.0f / 50.0f;
	uint8_t objectCount = 0;
	std::array<uint8_t, 51> padding{};
};

struct TelemetryRecord {
	float posX, posY, posZ;
	float velX, velY, velZ;
	float angVelX, angVelY, angVelZ;
	float rotX, rotY, rotZ, rotW;
};
#pragma pack(pop)

static_assert(sizeof(TelemetryHeader) == 64, "TelemetryHeader must be exactly 64 bytes");
static_assert(sizeof(TelemetryRecord) == 52, "TelemetryRecord must be exactly 52 bytes");

class TelemetryWriter {
public:
	TelemetryWriter(uint8_t scenarioId, uint8_t objectCount, float deltaTime = 1.0f / 50.0f);

	void AddFrame(const std::vector<TelemetryRecord>& frame);
	bool WriteToFile(const std::string& filePath) const;

private:
	TelemetryHeader mHeader;
	std::vector<TelemetryRecord> mRecords;
};

TelemetryRecord CaptureTelemetryRecord(BodyInterface& bodyInterface, BodyID bodyId);
