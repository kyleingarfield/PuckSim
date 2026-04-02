#include "Telemetry.h"

#include <Jolt/Physics/Body/BodyInterface.h>

#include <cstdio>

TelemetryWriter::TelemetryWriter(uint8_t scenarioId, uint8_t objectCount, float deltaTime)
{
	mHeader.version = 1;
	mHeader.scenarioId = scenarioId;
	mHeader.objectCount = objectCount;
	mHeader.deltaTime = deltaTime;
}

void TelemetryWriter::AddFrame(const std::vector<TelemetryRecord>& frame)
{
	if (frame.size() != mHeader.objectCount) {
		return;
	}

	mRecords.insert(mRecords.end(), frame.begin(), frame.end());
}

bool TelemetryWriter::WriteToFile(const std::string& filePath) const
{
	if (mHeader.objectCount == 0) {
		return false;
	}

	TelemetryHeader header = mHeader;
	header.frameCount = static_cast<uint32_t>(mRecords.size() / mHeader.objectCount);

	FILE* file = nullptr;
	if (fopen_s(&file, filePath.c_str(), "wb") != 0 || file == nullptr) {
		return false;
	}

	size_t written = fwrite(&header, sizeof(header), 1, file);
	if (!mRecords.empty()) {
		written += fwrite(mRecords.data(), sizeof(TelemetryRecord), mRecords.size(), file);
	}

	fclose(file);
	return written >= 1;
}

TelemetryRecord CaptureTelemetryRecord(BodyInterface& bodyInterface, BodyID bodyId)
{
	RVec3 pos = bodyInterface.GetPosition(bodyId);
	Vec3 vel = bodyInterface.GetLinearVelocity(bodyId);
	Vec3 angVel = bodyInterface.GetAngularVelocity(bodyId);
	Quat rot = bodyInterface.GetRotation(bodyId);

	TelemetryRecord record{};
	record.posX = pos.GetX();
	record.posY = pos.GetY();
	record.posZ = pos.GetZ();
	record.velX = vel.GetX();
	record.velY = vel.GetY();
	record.velZ = vel.GetZ();
	record.angVelX = angVel.GetX();
	record.angVelY = angVel.GetY();
	record.angVelZ = angVel.GetZ();
	record.rotX = rot.GetX();
	record.rotY = rot.GetY();
	record.rotZ = rot.GetZ();
	record.rotW = rot.GetW();
	return record;
}
