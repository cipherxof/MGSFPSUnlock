#include "mgs4.h"
#include "Memory.h"
#include "MinHook.h"
#include "Utils.h"
#include "config.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <mutex>

namespace
{
    using GetTargetFpsDelegate = int(__fastcall*)();
    using FrameTimeUpdateDelegate = void(__fastcall*)();
    using GamepadUpdateDelegate = void(__fastcall*)(uint32_t connectedMask, uint8_t* states, uint32_t* updatedMask, uint32_t resetMask, int32_t resetState);
    using GamepadVibrationSequenceUpdateDelegate = void(__fastcall*)(uint8_t* sequence);
    using GamepadVibrationMergeDelegate = void(__fastcall*)(uint32_t gamepadIndex, const uint8_t* samples, int32_t sampleCount);
    using SphericalCameraUpdateDelegate = void(__fastcall*)(uint8_t* camera);
    using PolygonDemoUpdateDelegate = void(__fastcall*)(uint8_t* demo);
    using MicrowaveMashControlUpdateDelegate = uint64_t(__fastcall*)(uint8_t* controller);
    using ActorMessagePollDelegate = int32_t(__fastcall*)(uint32_t key, uint8_t** messages);
    using WindManagerUpdateDelegate = void(__fastcall*)(uint8_t* windManager);
    using SpursTaskTimingDelegate = uint64_t(__fastcall*)(float* taskStep, uint32_t maxSteps);
    using ClothManagerUpdateDelegate = void(__fastcall*)(uint8_t* manager, float updateArgument, int32_t updateType);
    using ClothProducerUpdateDelegate = void(__fastcall*)(uint8_t* producer, float updateArgument, int32_t updateType);
    using ClothTransformPublishDelegate = void(__fastcall*)(uint8_t* producer);
    using DirectResidentClothOwnerUpdateDelegate = void(__fastcall*)(uint8_t* owner);
    using DirectResidentClothBonePublishDelegate = void(__fastcall*)(uint8_t* cloth, uint8_t** particleBuffer);
    using DirectResidentClothUpdateDelegate = void(__fastcall*)(uint8_t* cloth, uint8_t* context);
    using HairSimulationUpdateDelegate = void(__fastcall*)(uint8_t* hair);
    using PhysicsWorldTimeStepDelegate = void(__fastcall*)(uint8_t* world, float deltaTime);
    using NpcRagdollContactUpdateDelegate = void(__fastcall*)(uint8_t* result, uint8_t* character);
    using RagdollRadialForceDelegate = void(__fastcall*)(uint8_t* controller, const float* position, float strength);
    using RagdollBodyForceDelegate = void(__fastcall*)(uint8_t* controller, const float* position, float strength, int32_t bodyIndex);

    GetTargetFpsDelegate GetTargetFps = nullptr;
    FrameTimeUpdateDelegate FrameTimeUpdate = nullptr;
    GamepadUpdateDelegate GamepadUpdate = nullptr;
    GamepadVibrationSequenceUpdateDelegate GamepadVibrationSequenceUpdate = nullptr;
    GamepadVibrationMergeDelegate GamepadVibrationMerge = nullptr;
    SphericalCameraUpdateDelegate SphericalCameraUpdate = nullptr;
    PolygonDemoUpdateDelegate PolygonDemoUpdate = nullptr;
    MicrowaveMashControlUpdateDelegate MicrowaveMashControlUpdate = nullptr;
    ActorMessagePollDelegate ActorMessagePoll = nullptr;
    WindManagerUpdateDelegate WindManagerUpdate = nullptr;
    SpursTaskTimingDelegate SpursTaskTiming = nullptr;
    ClothManagerUpdateDelegate ClothManagerUpdate = nullptr;
    ClothProducerUpdateDelegate ClothProducerUpdate = nullptr;
    ClothTransformPublishDelegate ClothTransformPublish = nullptr;
    DirectResidentClothOwnerUpdateDelegate DirectResidentClothOwnerUpdate = nullptr;
    DirectResidentClothBonePublishDelegate DirectResidentClothBonePublish = nullptr;
    DirectResidentClothUpdateDelegate DirectResidentClothUpdate = nullptr;
    HairSimulationUpdateDelegate HairSimulationUpdate = nullptr;
    PhysicsWorldTimeStepDelegate PhysicsWorldTimeStep = nullptr;
    NpcRagdollContactUpdateDelegate NpcRagdollContactUpdate = nullptr;
    RagdollRadialForceDelegate RagdollRadialForce = nullptr;
    RagdollBodyForceDelegate RagdollBodyForce = nullptr;

    float* FrameDeltaSeconds = nullptr;
    int32_t* FrameTickDelta60 = nullptr;
    int32_t* FrameTickDelta300 = nullptr;
    int32_t* GamepadVibrationRawTickDelta300 = nullptr;
    uint8_t* GamepadVibrationOutputSamples = nullptr;
    double CharacterTickRemainder = 0.0;

    constexpr float ReferenceFps = 60.0f;
    constexpr float NativeSimulationStepSeconds = 0.016683351f;
    constexpr float CameraTurnDecay = 0.70710677f;
    constexpr float MaximumReasonableTaskStep = 0.1f;
    constexpr size_t GamepadCount = 4;
    constexpr size_t GamepadVibrationQueueSize = 0x40;
    constexpr size_t GamepadVibrationQueueBytes = GamepadCount * GamepadVibrationQueueSize;
    constexpr size_t GamepadMotorCount = 2;
    constexpr size_t PhysicsWorldTimeStateCount = 8;
    constexpr size_t PolygonDemoControlMessageCapacity = 16;
    constexpr size_t ActorMessageRecordSize = 0x18;
    constexpr ptrdiff_t MicrowaveInputModeOffset = 0x1E4;
    constexpr int32_t MashInputMode = 1;

    thread_local bool ActiveClothManagerTiming = false;
    thread_local bool ActiveClothProducerTiming = false;
    thread_local bool ActiveBandanaTiming = false;
    thread_local bool ActiveManagerStripPerRenderTiming = false;
    thread_local bool ActiveNpcRagdollContactUpdate = false;

    struct PhysicsWorldTimeState
    {
        uintptr_t world = 0;
        float fixedStep = 0.0f;
        double accumulator = 0.0;
    };

    struct ClothObjectListView
    {
        uint8_t* first = nullptr;
        uint32_t count = 0;
    };

    struct PolygonDemoControlMessageQueue
    {
        uintptr_t demo = 0;
        uint32_t key = 0;
        size_t count = 0;
        std::array<int32_t, PolygonDemoControlMessageCapacity> codes{};
    };

    Memory::ModuleSection GameText{};
    std::array<PhysicsWorldTimeState, PhysicsWorldTimeStateCount> PhysicsWorldTimeStates{};
    std::array<uint8_t, GamepadVibrationQueueBytes> ActiveGamepadVibrationSamples{};
    std::array<uint8_t, GamepadVibrationQueueBytes> PendingGamepadVibrationSamples{};
    std::atomic<uint64_t> RenderFrameSerial{0};
    std::atomic<uint64_t> PolygonDemoFrameSerial{0};
    std::mutex GamepadVibrationMutex;
    thread_local uint8_t* ActivePolygonDemoUpdate = nullptr;
    thread_local bool DeferPolygonDemoControlMessages = false;
    thread_local PolygonDemoControlMessageQueue PendingPolygonDemoControlMessages;
    alignas(8) thread_local std::array<uint8_t, PolygonDemoControlMessageCapacity * ActorMessageRecordSize>
        DeferredPolygonDemoMessageRecords{};
    thread_local std::array<int32_t, PolygonDemoControlMessageCapacity> DeferredPolygonDemoMessagePayloads{};

    bool IsNativeTick()
    {
        return Config.targetFramerate <= ReferenceFps || !FrameTickDelta60 || *FrameTickDelta60 != 0;
    }

    bool IsPolygonDemoActive()
    {
        const uint64_t currentFrame = RenderFrameSerial.load(std::memory_order_relaxed);
        const uint64_t polygonDemoFrame = PolygonDemoFrameSerial.load(std::memory_order_relaxed);
        return polygonDemoFrame != 0 && currentFrame >= polygonDemoFrame && currentFrame - polygonDemoFrame <= 1;
    }

    ClothObjectListView GetClothObjectList(const uint8_t* owner, ptrdiff_t listOffset)
    {
        if (!owner)
            return {};

        const uint8_t* list = *reinterpret_cast<uint8_t* const*>(owner + listOffset);
        if (!list)
            return {};

        const int32_t count = *reinterpret_cast<const int32_t*>(list);
        if (count <= 0 || count > 4096)
            return {};

        uint8_t* const* objects = *reinterpret_cast<uint8_t* const* const*>(list + 0x10);
        return {objects ? objects[0] : nullptr, static_cast<uint32_t>(count)};
    }

    uint16_t GetStripSolverPointCount(const uint8_t* solver)
    {
        return solver ? *reinterpret_cast<const uint16_t*>(solver + 0x110) : 0;
    }

    int __fastcall GetTargetFpsHook()
    {
        return Config.targetFramerate;
    }

    void __fastcall FrameTimeUpdateHook()
    {
        FrameTimeUpdate();
        RenderFrameSerial.fetch_add(1, std::memory_order_relaxed);

        if (Config.targetFramerate <= ReferenceFps || !FrameDeltaSeconds || !FrameTickDelta300)
        {
            CharacterTickRemainder = 0.0;
            return;
        }

        const float exactTicks = *FrameDeltaSeconds * 300.0f;
        if (exactTicks < 1.0f || !std::isfinite(exactTicks))
        {
            CharacterTickRemainder = 0.0;
            return;
        }

        const double accumulatedTicks = CharacterTickRemainder + exactTicks;
        const int32_t wholeTicks = static_cast<int32_t>(accumulatedTicks);
        CharacterTickRemainder = accumulatedTicks - wholeTicks;
        *FrameTickDelta300 = wholeTicks;
    }

    void MergeGamepadVibrationSamples(uint8_t* destination, const uint8_t* source, size_t count)
    {
        for (size_t index = 0; index < count; ++index)
            destination[index] = std::max(destination[index], source[index]);
    }

    void __fastcall GamepadVibrationSequenceUpdateHook(uint8_t* sequence)
    {
        if (Config.targetFramerate <= ReferenceFps || !FrameTickDelta60 || !FrameTickDelta300 || !GamepadVibrationRawTickDelta300)
        {
            GamepadVibrationSequenceUpdate(sequence);
            return;
        }

        const int32_t nativeTickCount = *FrameTickDelta60;
        if (nativeTickCount <= 0)
            return;

        const int32_t originalRawTicks = *GamepadVibrationRawTickDelta300;
        const int32_t originalScaledTicks = *FrameTickDelta300;
        const int32_t rawTicks = std::clamp(nativeTickCount, 1, 6) * 5;
        int32_t scaledTicks = rawTicks;
        if (originalRawTicks > 0)
        {
            const double timeScale = static_cast<double>(originalScaledTicks) / originalRawTicks;
            scaledTicks = std::max(1, static_cast<int32_t>(std::lround(rawTicks * timeScale)));
        }

        *GamepadVibrationRawTickDelta300 = rawTicks;
        *FrameTickDelta300 = scaledTicks;
        GamepadVibrationSequenceUpdate(sequence);
        *GamepadVibrationRawTickDelta300 = originalRawTicks;
        *FrameTickDelta300 = originalScaledTicks;
    }

    void __fastcall GamepadVibrationMergeHook(uint32_t gamepadIndex, const uint8_t* samples, int32_t sampleCount)
    {
        if (Config.targetFramerate > ReferenceFps && gamepadIndex < GamepadCount && samples && sampleCount > 0)
        {
            const size_t samplesToMerge = static_cast<size_t>(std::min(sampleCount, 32));
            uint8_t* destination = PendingGamepadVibrationSamples.data() + gamepadIndex * GamepadVibrationQueueSize;
            std::lock_guard<std::mutex> lock(GamepadVibrationMutex);
            MergeGamepadVibrationSamples(destination, samples, samplesToMerge * 2);

            if (samplesToMerge > 1)
            {
                for (size_t motor = 0; motor < GamepadMotorCount; ++motor)
                {
                    if (samples[motor] == 0 || samples[GamepadMotorCount + motor] != 0)
                        continue;

                    destination[GamepadMotorCount + motor] =
                        std::max(destination[GamepadMotorCount + motor], samples[motor]);
                }
            }
        }

        GamepadVibrationMerge(gamepadIndex, samples, sampleCount);
    }

    void __fastcall GamepadUpdateHook(uint32_t connectedMask, uint8_t* states, uint32_t* updatedMask, uint32_t resetMask, int32_t resetState)
    {
        if (Config.targetFramerate <= ReferenceFps || !GamepadVibrationOutputSamples)
        {
            GamepadUpdate(connectedMask, states, updatedMask, resetMask, resetState);
            return;
        }

        std::lock_guard<std::mutex> lock(GamepadVibrationMutex);
        MergeGamepadVibrationSamples(ActiveGamepadVibrationSamples.data(), PendingGamepadVibrationSamples.data(), GamepadVibrationQueueBytes);
        PendingGamepadVibrationSamples.fill(0);
        std::memcpy(GamepadVibrationOutputSamples, ActiveGamepadVibrationSamples.data(), ActiveGamepadVibrationSamples.size());

        GamepadUpdate(connectedMask, states, updatedMask, resetMask, resetState);

        std::memcpy(ActiveGamepadVibrationSamples.data(), GamepadVibrationOutputSamples, ActiveGamepadVibrationSamples.size());
    }

    void __fastcall SphericalCameraUpdateHook(uint8_t* camera)
    {
        if (!camera || !FrameDeltaSeconds)
        {
            SphericalCameraUpdate(camera);
            return;
        }

        const float deltaTime = *FrameDeltaSeconds;
        if (!(deltaTime > 0.0f) || !std::isfinite(deltaTime))
        {
            SphericalCameraUpdate(camera);
            return;
        }

        const float frameDecay = std::pow(CameraTurnDecay, deltaTime * ReferenceFps);
        const float turnRateScale = (1.0f - frameDecay) / (1.0f - CameraTurnDecay);
        float* turnSpeed = reinterpret_cast<float*>(camera + 0xC0);
        const float originalTurnSpeed = *turnSpeed;
        *turnSpeed *= turnRateScale;
        SphericalCameraUpdate(camera);
        *turnSpeed = originalTurnSpeed;
    }

    int32_t __fastcall ActorMessagePollHook(uint32_t key, uint8_t** messages)
    {
        int32_t result = ActorMessagePoll(key, messages);
        uint8_t* const demo = ActivePolygonDemoUpdate;
        if (!demo || !messages || key != *reinterpret_cast<const uint32_t*>(demo + 0x44530))
            return result;

        PolygonDemoControlMessageQueue& pending = PendingPolygonDemoControlMessages;
        const uintptr_t demoAddress = reinterpret_cast<uintptr_t>(demo);
        if (pending.count != 0 && (pending.demo != demoAddress || pending.key != key))
            pending = {};

        if (DeferPolygonDemoControlMessages)
        {
            if (result > 0 && *messages)
            {
                if (pending.count == 0)
                {
                    pending.demo = demoAddress;
                    pending.key = key;
                }

                const size_t available = PolygonDemoControlMessageCapacity - pending.count;
                const size_t captured = std::min(static_cast<size_t>(result), available);
                for (size_t index = 0; index < captured; ++index)
                {
                    const uint8_t* const record = *messages + index * ActorMessageRecordSize;
                    const int32_t* const payload = *reinterpret_cast<int32_t* const*>(record + 0x08);
                    if (payload)
                        pending.codes[pending.count++] = payload[0];
                }
            }

            *messages = nullptr;
            return 0;
        }

        if (pending.count == 0)
            return result;

        const size_t nativeCount = result > 0 ? static_cast<size_t>(result) : 0;
        if ((nativeCount != 0 && !*messages) ||
            nativeCount + pending.count > PolygonDemoControlMessageCapacity)
        {
            return result;
        }

        DeferredPolygonDemoMessageRecords.fill(0);
        if (nativeCount != 0)
        {
            std::memcpy(
                DeferredPolygonDemoMessageRecords.data(),
                *messages,
                nativeCount * ActorMessageRecordSize);
        }

        for (size_t index = 0; index < pending.count; ++index)
        {
            const size_t recordIndex = nativeCount + index;
            uint8_t* const record =
                DeferredPolygonDemoMessageRecords.data() + recordIndex * ActorMessageRecordSize;
            DeferredPolygonDemoMessagePayloads[recordIndex] = pending.codes[index];
            int32_t* const payload = &DeferredPolygonDemoMessagePayloads[recordIndex];
            std::memcpy(record + 0x08, &payload, sizeof(payload));
        }

        result = static_cast<int32_t>(nativeCount + pending.count);
        *messages = DeferredPolygonDemoMessageRecords.data();
        pending = {};
        return result;
    }

    void CapturePolygonDemoControlMessages(uint8_t* demo)
    {
        if (!demo || !ActorMessagePoll)
            return;

        const uint32_t key = *reinterpret_cast<const uint32_t*>(demo + 0x44530);
        if (key == 0)
            return;

        uint8_t* messages = nullptr;
        uint8_t* const previousDemo = ActivePolygonDemoUpdate;
        const bool previousDefer = DeferPolygonDemoControlMessages;
        ActivePolygonDemoUpdate = demo;
        DeferPolygonDemoControlMessages = true;
        ActorMessagePollHook(key, &messages);
        DeferPolygonDemoControlMessages = previousDefer;
        ActivePolygonDemoUpdate = previousDemo;
    }

    void __fastcall PolygonDemoUpdateHook(uint8_t* demo)
    {
        PolygonDemoFrameSerial.store(RenderFrameSerial.load(std::memory_order_relaxed), std::memory_order_relaxed);
        if (!demo)
        {
            if (IsNativeTick())
                PolygonDemoUpdate(demo);
            return;
        }

        const bool timelineReset = *reinterpret_cast<int32_t*>(demo + 0x44588) != 0;
        if (timelineReset)
            PendingPolygonDemoControlMessages = {};

        const bool nativeUpdate = timelineReset || IsNativeTick();
        const bool pauseMaintenanceUpdate =
            !nativeUpdate && *reinterpret_cast<int32_t*>(demo + 0x445A0) != 0;
        if (!nativeUpdate && !pauseMaintenanceUpdate)
        {
            // Actor message buffers rotate every render frame. Preserve control messages that would otherwise expire before the next update.
            CapturePolygonDemoControlMessages(demo);
            return;
        }

        uint8_t* const previousDemo = ActivePolygonDemoUpdate;
        const bool previousDefer = DeferPolygonDemoControlMessages;
        int32_t savedCountdown = 0;
        if (pauseMaintenanceUpdate)
        {
            savedCountdown = *reinterpret_cast<int32_t*>(demo + 0x445C4);
            *reinterpret_cast<int32_t*>(demo + 0x445C4) = 0;
            DeferPolygonDemoControlMessages = true;
        }

        ActivePolygonDemoUpdate = demo;
        PolygonDemoUpdate(demo);
        ActivePolygonDemoUpdate = previousDemo;
        DeferPolygonDemoControlMessages = previousDefer;

        if (pauseMaintenanceUpdate)
            *reinterpret_cast<int32_t*>(demo + 0x445C4) = savedCountdown;
    }

    void __fastcall WindManagerUpdateHook(uint8_t* windManager)
    {
        if (IsNativeTick())
            WindManagerUpdate(windManager);
    }

    uint64_t __fastcall MicrowaveMashControlUpdateHook(uint8_t* controller)
    {
        const bool mashInputActive = controller &&
            *reinterpret_cast<const int32_t*>(controller + MicrowaveInputModeOffset) == MashInputMode;
        if (mashInputActive && !IsNativeTick())
            return 0;

        return MicrowaveMashControlUpdate(controller);
    }

    uint64_t __fastcall SpursTaskTimingHook(float* taskStep, uint32_t maxSteps)
    {
        uint64_t stepCount = SpursTaskTiming(taskStep, maxSteps);
        if (!taskStep)
            return stepCount;

        const float stockStep = *taskStep;
        const float exactDelta = FrameDeltaSeconds ? *FrameDeltaSeconds : 0.0f;
        const bool validExactDelta = exactDelta > 0.0f && exactDelta <= MaximumReasonableTaskStep && std::isfinite(exactDelta);

        if (ActiveBandanaTiming && Config.targetFramerate > ReferenceFps && maxSteps != 0)
        {
            *taskStep = NativeSimulationStepSeconds;
            stepCount = std::min(1u, maxSteps);
        }
        else
        {
            const bool useExactDelta = (!ActiveClothManagerTiming || ActiveManagerStripPerRenderTiming) &&
                                       !ActiveClothProducerTiming && validExactDelta &&
                                       stockStep > exactDelta && stockStep <= MaximumReasonableTaskStep && std::isfinite(stockStep);
            if (useExactDelta)
            {
                *taskStep = exactDelta;
                stepCount = 1;
            }
        }

        return stepCount;
    }

    void __fastcall ClothManagerUpdateHook(uint8_t* manager, float updateArgument, int32_t updateType)
    {
        const bool previousTiming = ActiveClothManagerTiming;
        ActiveClothManagerTiming = true;
        ClothManagerUpdate(manager, updateArgument, updateType);
        ActiveClothManagerTiming = previousTiming;
    }

    void __fastcall ClothProducerUpdateHook(uint8_t* producer, float updateArgument, int32_t updateType)
    {
        const bool nativeTick = IsNativeTick();
        const ClothObjectListView objects = GetClothObjectList(producer, 0x08);
        const uint32_t pointCount = GetStripSolverPointCount(objects.first);
        const bool managerBacked = ActiveClothManagerTiming;
        const bool cutsceneSevenPoint = pointCount == 7 && IsPolygonDemoActive();
        const bool managerBackedHair = managerBacked && objects.count == 1 && pointCount == 11;
        const bool usePerRenderDelta = managerBackedHair || (!managerBacked && (pointCount == 10 || (pointCount == 7 && !cutsceneSevenPoint)));
        const bool simulated = usePerRenderDelta || nativeTick;

        if (simulated)
        {
            const bool previousTiming = ActiveClothProducerTiming;
            const bool previousManagerPerRenderTiming = ActiveManagerStripPerRenderTiming;
            ActiveClothProducerTiming = !usePerRenderDelta;
            ActiveManagerStripPerRenderTiming = managerBackedHair;
            ClothProducerUpdate(producer, updateArgument, updateType);
            ActiveManagerStripPerRenderTiming = previousManagerPerRenderTiming;
            ActiveClothProducerTiming = previousTiming;
        }
        else if (producer && ClothTransformPublish)
        {
            ClothTransformPublish(producer);
        }
    }

    void PublishDirectResidentClothTransform(uint8_t* cloth)
    {
        if (!cloth)
            return;

        const uint8_t* sourceTransform = *reinterpret_cast<uint8_t**>(cloth + 0x98);
        if (sourceTransform)
            std::memcpy(cloth + 0x4B0, sourceTransform, 0x40);
    }

    void PublishDirectResidentClothBones(uint8_t* owner, uint8_t* cloth)
    {
        if (!owner || !cloth || !DirectResidentClothBonePublish)
            return;

        const uint32_t pingPong = *reinterpret_cast<uint32_t*>(owner + 0xC0) & 1;
        uint8_t** particleBufferSlot = reinterpret_cast<uint8_t**>(
            cloth + 0x08 + pingPong * sizeof(uint8_t*));
        if (!*particleBufferSlot)
            return;

        uint8_t* particleBuffer = *particleBufferSlot;
        DirectResidentClothBonePublish(cloth, &particleBuffer);
    }

    void __fastcall DirectResidentClothOwnerUpdateHook(uint8_t* owner)
    {
        uint8_t* cloth = owner ? *reinterpret_cast<uint8_t**>(owner + 0x90) : nullptr;
        const uint16_t pointCount = cloth ? *reinterpret_cast<uint16_t*>(cloth) : 0;
        const bool directCloth = pointCount != 0;
        const bool nativeTick = IsNativeTick();

        if (directCloth && Config.targetFramerate > ReferenceFps && !nativeTick)
        {
            int32_t* simulationGate = reinterpret_cast<int32_t*>(owner + 0xC4);
            const int32_t originalSimulationGate = *simulationGate;

            *simulationGate = 1;
            DirectResidentClothOwnerUpdate(owner);
            *simulationGate = originalSimulationGate;
            PublishDirectResidentClothBones(owner, cloth);
            PublishDirectResidentClothTransform(cloth);
        }
        else
        {
            DirectResidentClothOwnerUpdate(owner);
        }
    }

    void __fastcall DirectResidentClothUpdateHook(uint8_t* cloth, uint8_t* context)
    {
        const uint16_t pointCount = cloth ? *reinterpret_cast<uint16_t*>(cloth) : 0;
        const bool fixedRate = pointCount != 0 && Config.targetFramerate > ReferenceFps && FrameTickDelta60;
        const bool nativeTick = IsNativeTick();

        if (!fixedRate)
        {
            DirectResidentClothUpdate(cloth, context);
        }
        else if (!nativeTick)
        {
            PublishDirectResidentClothTransform(cloth);
        }
        else if (!context)
        {
            DirectResidentClothUpdate(cloth, context);
        }
        else
        {
            float* deltaTime = reinterpret_cast<float*>(context + 0x30);
            float* reciprocalDelta = reinterpret_cast<float*>(context + 0x34);
            const float originalDelta = *deltaTime;
            const float originalReciprocalDelta = *reciprocalDelta;
            *deltaTime = NativeSimulationStepSeconds;
            *reciprocalDelta = 1.0f / NativeSimulationStepSeconds;
            DirectResidentClothUpdate(cloth, context);
            *deltaTime = originalDelta;
            *reciprocalDelta = originalReciprocalDelta;
        }
    }

    void __fastcall HairSimulationUpdateHook(uint8_t* hair)
    {
        const uint32_t chainCount = hair ? *reinterpret_cast<uint32_t*>(hair + 0x260) : 0;
        const bool previousBandanaTiming = ActiveBandanaTiming;
        ActiveBandanaTiming = chainCount == 17 && Config.targetFramerate > ReferenceFps;
        HairSimulationUpdate(hair);
        ActiveBandanaTiming = previousBandanaTiming;
    }

    PhysicsWorldTimeState* GetPhysicsWorldTimeState(uint8_t* world, float fixedStep)
    {
        const uintptr_t worldAddress = reinterpret_cast<uintptr_t>(world);
        const size_t first = (worldAddress >> 4) & (PhysicsWorldTimeStateCount - 1);

        for (size_t probe = 0; probe < PhysicsWorldTimeStateCount; ++probe)
        {
            PhysicsWorldTimeState& state = PhysicsWorldTimeStates[(first + probe) & (PhysicsWorldTimeStateCount - 1)];
            if (state.world == worldAddress)
            {
                if (state.fixedStep != fixedStep)
                {
                    state.fixedStep = fixedStep;
                    state.accumulator = 0.0;
                }
                return &state;
            }

            if (state.world == 0)
            {
                state.world = worldAddress;
                state.fixedStep = fixedStep;
                state.accumulator = 0.0;
                return &state;
            }
        }

        PhysicsWorldTimeState& state = PhysicsWorldTimeStates[first];
        state.world = worldAddress;
        state.fixedStep = fixedStep;
        state.accumulator = 0.0;
        return &state;
    }

    void __fastcall PhysicsWorldTimeStepHook(uint8_t* world, float deltaTime)
    {
        if (!world || !(deltaTime > 0.0f) || !std::isfinite(deltaTime))
        {
            PhysicsWorldTimeStep(world, deltaTime);
            return;
        }

        const float fixedStep = *reinterpret_cast<float*>(world + 0x670);
        if (!(fixedStep > 0.0f) || fixedStep >= 1.0f || !std::isfinite(fixedStep))
        {
            PhysicsWorldTimeStep(world, deltaTime);
            return;
        }

        PhysicsWorldTimeState* state = GetPhysicsWorldTimeState(world, fixedStep);
        state->accumulator += static_cast<double>(deltaTime);
        uint32_t stepCount = static_cast<uint32_t>(state->accumulator / static_cast<double>(fixedStep) + 0.00001);
        stepCount = std::min(stepCount, 2u);
        if (stepCount == 0)
            return;

        const float simulatedTime = stepCount * fixedStep;
        state->accumulator -= static_cast<double>(simulatedTime);
        state->accumulator = std::max(state->accumulator, 0.0);
        PhysicsWorldTimeStep(world, simulatedTime);
    }

    void ScaleNpcRagdollVelocity(uint8_t* ragdoll, float scale)
    {
        if (!ragdoll)
            return;

        for (size_t index = 0; index < 13; ++index)
        {
            uint8_t* body = *reinterpret_cast<uint8_t**>(ragdoll + 8 + index * sizeof(uintptr_t));
            if (!body)
                continue;

            float* linearVelocity = reinterpret_cast<float*>(body + 0x60);
            float* angularVelocity = reinterpret_cast<float*>(body + 0x70);
            for (size_t component = 0; component < 3; ++component)
            {
                linearVelocity[component] *= scale;
                angularVelocity[component] *= scale;
            }
        }
    }

    void NormalizeActiveNpcRagdollVelocity(uint8_t* controller)
    {
        const float frameDelta = FrameDeltaSeconds ? *FrameDeltaSeconds : 0.0f;
        const float velocityScale = frameDelta * ReferenceFps;

        if (ActiveNpcRagdollContactUpdate && velocityScale > 0.0f && velocityScale < 1.0f && std::isfinite(velocityScale))
        {
            uint8_t* ragdoll = controller ? *reinterpret_cast<uint8_t**>(controller + 0x30) : nullptr;
            ScaleNpcRagdollVelocity(ragdoll, velocityScale);
        }
    }

    void __fastcall RagdollRadialForceHook(uint8_t* controller, const float* position, float strength)
    {
        NormalizeActiveNpcRagdollVelocity(controller);
        RagdollRadialForce(controller, position, strength);
    }

    void __fastcall RagdollBodyForceHook(uint8_t* controller, const float* position, float strength, int32_t bodyIndex)
    {
        NormalizeActiveNpcRagdollVelocity(controller);
        RagdollBodyForce(controller, position, strength, bodyIndex);
    }

    void __fastcall NpcRagdollContactUpdateHook(uint8_t* result, uint8_t* character)
    {
        const bool wasActive = ActiveNpcRagdollContactUpdate;
        ActiveNpcRagdollContactUpdate = true;
        NpcRagdollContactUpdate(result, character);
        ActiveNpcRagdollContactUpdate = wasActive;
    }

    uint8_t* FindTargetFps()
    {
        constexpr char TargetFpsPattern[] = "40 55 48 8B EC 48 83 EC 50 8B 05 ?? ?? ?? ?? 83 F8 FF 0F 85 ?? ?? ?? ?? E8 ?? ?? ?? ?? 3D 80 00 00 00";

        if (!Memory::GetModuleSection(GameModule, ".text", GameText))
            return nullptr;

        return Memory::PatternScanRange(GameText.begin, GameText.size, TargetFpsPattern);
    }

    bool InstallCharacterControlTimingFix()
    {
        constexpr char Pattern[] = "40 53 48 83 EC 20 83 3D ?? ?? ?? ?? 00 BB 01 00 00 00 75 ?? 89 1D ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 89 05 ?? ?? ?? ?? E8";

        if (!FrameDeltaSeconds)
        {
            spdlog::error("Character timing requires the frame delta");
            return false;
        }

        uint8_t* frameTimeUpdate = Memory::PatternScanRange(GameText.begin, GameText.size, Pattern);
        if (!frameTimeUpdate)
            return false;
        LogAddress("frameTimeUpdate", reinterpret_cast<uintptr_t>(frameTimeUpdate));

        FrameTickDelta300 = reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(FrameDeltaSeconds) + 0xC);
        LogAddress("frameTickDelta300", reinterpret_cast<uintptr_t>(FrameTickDelta300));

        if (MH_CreateHook(frameTimeUpdate, reinterpret_cast<LPVOID>(&FrameTimeUpdateHook), reinterpret_cast<void**>(&FrameTimeUpdate)) != MH_OK)
            return false;

        spdlog::info("Character control timing fix installed");
        return true;
    }

    bool InstallGamepadVibrationTimingFix()
    {
        constexpr char UpdatePattern[] = "44 89 4C 24 20 4C 89 44 24 18 48 89 54 24 10 89 4C 24 08 53 55 56 57 41 54 41 55 41 56 41 57 48 81 EC 08 01 00 00 45 33 ED";
        constexpr char SequenceUpdatePattern[] = "40 53 41 57 48 83 EC 48 48 8B D9 0F 29 74 24 30 48 8B 89 F8 00 00 00 F3 0F 10 35 ?? ?? ?? ?? 48 85 C9 74 0D 8B 01 25 00 10 00 00 0F 85 ?? ?? ?? ?? 48 89 74 24 70 B9 01 00 00 00";
        constexpr char MergePattern[] = "48 8D 05 ?? ?? ?? ?? 44 8B D1 49 C1 E2 06 45 8B D8 4C 03 D0 4C 8B CA 33 C0 0F 1F 80 00 00 00 00 41 0F B6 0C 42 41 FF CB";
        constexpr std::array<uint8_t, 3> OutputQueueLoadOpcode = {0x48, 0x8D, 0x1D};
        constexpr std::array<uint8_t, 2> RawTickLoadOpcode = {0xF7, 0x2D};
        constexpr std::array<uint8_t, 4> ScaledTickLoadOpcode = {0x66, 0x0F, 0x6E, 0x05};
        constexpr ptrdiff_t RawTickLoadOffset = 0x56;
        constexpr ptrdiff_t ScaledTickLoadOffset = 0x64;

        uint8_t* update = Memory::PatternScanRange(GameText.begin, GameText.size, UpdatePattern);
        uint8_t* sequenceUpdate = Memory::PatternScanRange(GameText.begin, GameText.size, SequenceUpdatePattern);
        uint8_t* merge = Memory::PatternScanRange(GameText.begin, GameText.size, MergePattern);

        if (!update || !sequenceUpdate || !merge)
            return false;

        LogAddress("gamepadUpdate", reinterpret_cast<uintptr_t>(update));
        LogAddress("gamepadVibrationSequenceUpdate", reinterpret_cast<uintptr_t>(sequenceUpdate));
        LogAddress("gamepadVibrationMerge", reinterpret_cast<uintptr_t>(merge));

        if (std::memcmp(sequenceUpdate + RawTickLoadOffset, RawTickLoadOpcode.data(), RawTickLoadOpcode.size()) != 0 ||
            std::memcmp(sequenceUpdate + ScaledTickLoadOffset, ScaledTickLoadOpcode.data(), ScaledTickLoadOpcode.size()) != 0)
        {
            spdlog::error("Gamepad vibration sequence tick-load validation failed");
            return false;
        }

        int32_t rawTickDisplacement = 0;
        int32_t scaledTickDisplacement = 0;
        std::memcpy(&rawTickDisplacement, sequenceUpdate + 0x58, sizeof(rawTickDisplacement));
        std::memcpy(&scaledTickDisplacement, sequenceUpdate + 0x68, sizeof(scaledTickDisplacement));
        GamepadVibrationRawTickDelta300 = reinterpret_cast<int32_t*>(sequenceUpdate + RawTickLoadOffset + 6 + rawTickDisplacement);
        int32_t* sequenceScaledTickDelta300 = reinterpret_cast<int32_t*>(sequenceUpdate + ScaledTickLoadOffset + 8 + scaledTickDisplacement);
        LogAddress("gamepadVibrationRawTickDelta300", reinterpret_cast<uintptr_t>(GamepadVibrationRawTickDelta300));
        if (sequenceScaledTickDelta300 != FrameTickDelta300)
        {
            spdlog::error("Gamepad vibration scaled tick is not the known 300 Hz frame tick");
            return false;
        }

        uint8_t* outputQueueLoad = update + 0x64F;
        if (std::memcmp(outputQueueLoad, OutputQueueLoadOpcode.data(), OutputQueueLoadOpcode.size()) != 0)
        {
            spdlog::error("Gamepad vibration output-queue load validation failed");
            return false;
        }

        int32_t outputDisplacement = 0;
        std::memcpy(&outputDisplacement, outputQueueLoad + 3, sizeof(outputDisplacement));
        GamepadVibrationOutputSamples = outputQueueLoad + 7 + outputDisplacement;
        LogAddress("gamepadVibrationOutputSamples", reinterpret_cast<uintptr_t>(GamepadVibrationOutputSamples));

        if (MH_CreateHook(update, reinterpret_cast<LPVOID>(&GamepadUpdateHook), reinterpret_cast<void**>(&GamepadUpdate)) != MH_OK)
            return false;

        if (MH_CreateHook(sequenceUpdate, reinterpret_cast<LPVOID>(&GamepadVibrationSequenceUpdateHook), reinterpret_cast<void**>(&GamepadVibrationSequenceUpdate)) != MH_OK)
            return false;

        if (MH_CreateHook(merge, reinterpret_cast<LPVOID>(&GamepadVibrationMergeHook), reinterpret_cast<void**>(&GamepadVibrationMerge)) != MH_OK)
            return false;

        spdlog::info("Gamepad vibration timing fix applied");
        return true;
    }

    bool InstallSphericalCameraTimingFix()
    {
        constexpr char Pattern[] = "48 89 5C 24 10 56 48 83 EC 30 48 8B D9 48 89 7C 24 40 8B 89 A8 00 00 00 33 F6 85 C9 0F 84 ?? ?? ?? ?? 83 E9 01 0F 84 ?? ?? ?? ?? 83 E9 01 74 ?? 83 F9 01 0F 85 ?? ?? ?? ?? F3 0F 10 05 ?? ?? ?? ??";
        constexpr ptrdiff_t DeltaLoadOffset = 0x39;
        constexpr std::array<uint8_t, 4> DeltaLoadOpcode = {0xF3, 0x0F, 0x10, 0x05};

        uint8_t* update = Memory::PatternScanRange(GameText.begin, GameText.size, Pattern);
        if (!update)
            return false;
        LogAddress("sphericalCameraUpdate", reinterpret_cast<uintptr_t>(update));
        if (std::memcmp(update + DeltaLoadOffset, DeltaLoadOpcode.data(), DeltaLoadOpcode.size()) != 0)
        {
            spdlog::error("Spherical camera delta-time load validation failed");
            return false;
        }

        int32_t displacement = 0;
        std::memcpy(&displacement, update + 0x3D, sizeof(displacement));
        FrameDeltaSeconds = reinterpret_cast<float*>(update + DeltaLoadOffset + 8 + displacement);
        LogAddress("frameDeltaSeconds", reinterpret_cast<uintptr_t>(FrameDeltaSeconds));

        if (MH_CreateHook(update, reinterpret_cast<LPVOID>(&SphericalCameraUpdateHook), reinterpret_cast<void**>(&SphericalCameraUpdate)) != MH_OK)
        {
            FrameDeltaSeconds = nullptr;
            return false;
        }

        spdlog::info("Spherical camera turn rate normalized");
        return true;
    }

    bool InstallPolygonDemoTimingFix()
    {
        constexpr char Pattern[] = "48 89 5C 24 20 56 57 41 54 41 56 41 57 48 83 EC 40 48 8B 1D ?? ?? ?? ?? 4C 8D B1 90 00 00 00 33 F6 4C 8D 3D ?? ?? ?? ?? 48 8B F9 8B 89 C4 45 04 00 44 8D 66 01 8D 56 02 85 C9 7E ?? 3B 8F C0 45 04 00";
        constexpr char ActorMessagePollPattern[] = "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 41 54 41 55 41 56 41 57 48 83 EC 20 4C 8B E2 8B F1 E8 ?? ?? ?? ?? 85 C0";
        constexpr std::array<uint8_t, 3> TimeDeltaLoadOpcode = {0x4C, 0x8D, 0x3D};

        uint8_t* update = Memory::PatternScanRange(GameText.begin, GameText.size, Pattern);
        uint8_t* actorMessagePoll = Memory::PatternScanRange(GameText.begin, GameText.size, ActorMessagePollPattern);
        if (!update || !actorMessagePoll)
            return false;
        LogAddress("polygonDemoUpdate", reinterpret_cast<uintptr_t>(update));
        LogAddress("actorMessagePoll", reinterpret_cast<uintptr_t>(actorMessagePoll));
        if (std::memcmp(update + 0x21, TimeDeltaLoadOpcode.data(), TimeDeltaLoadOpcode.size()) != 0)
        {
            spdlog::error("Polygon-demo time-delta load validation failed");
            return false;
        }

        uint8_t* timeDelta = reinterpret_cast<uint8_t*>(GetRelativeOffset(update + 0x24));
        FrameTickDelta60 = reinterpret_cast<int32_t*>(timeDelta + 8);
        LogAddress("frameTickDelta60", reinterpret_cast<uintptr_t>(FrameTickDelta60));

        if (MH_CreateHook(update, reinterpret_cast<LPVOID>(&PolygonDemoUpdateHook), reinterpret_cast<void**>(&PolygonDemoUpdate)) != MH_OK)
        {
            FrameTickDelta60 = nullptr;
            return false;
        }

        if (MH_CreateHook(actorMessagePoll, reinterpret_cast<LPVOID>(&ActorMessagePollHook), reinterpret_cast<void**>(&ActorMessagePoll)) != MH_OK)
        {
            MH_RemoveHook(update);
            PolygonDemoUpdate = nullptr;
            FrameTickDelta60 = nullptr;
            return false;
        }

        spdlog::info("Polygon-demo timing and control-message fixes applied");
        return true;
    }

    bool InstallWindManagerTimingFix()
    {
        constexpr char Pattern[] = "40 53 48 81 EC 80 00 00 00 48 8B D9 E8 ?? ?? ?? ?? 8B 05 ?? ?? ?? ?? 0B 05 ?? ?? ?? ?? A9 00 00 80 10 74";
        uint8_t* update = Memory::PatternScanRange(GameText.begin, GameText.size, Pattern);
        if (!update)
            return false;
        LogAddress("windManagerUpdate", reinterpret_cast<uintptr_t>(update));

        if (MH_CreateHook(update, reinterpret_cast<LPVOID>(&WindManagerUpdateHook), reinterpret_cast<void**>(&WindManagerUpdate)) != MH_OK)
            return false;

        spdlog::info("Global wind timing fix applied");
        return true;
    }

    bool InstallMicrowaveMashTimingFix()
    {
        constexpr char Pattern[] = "48 89 5C 24 10 48 89 6C 24 18 56 57 41 54 41 56 41 57 48 83 EC 40 48 8B 69 38 48 8B D9 8B 89 E4 01 00 00 45 33 FF 41 BC FF 00 00 00";
        uint8_t* update = Memory::PatternScanRange(GameText.begin, GameText.size, Pattern);
        if (!update)
            return false;
        LogAddress("microwaveMashControlUpdate", reinterpret_cast<uintptr_t>(update));

        if (MH_CreateHook(update, reinterpret_cast<LPVOID>(&MicrowaveMashControlUpdateHook), reinterpret_cast<void**>(&MicrowaveMashControlUpdate)) != MH_OK)
            return false;

        spdlog::info("Microwave mash-input timing fix applied");
        return true;
    }

    bool InstallSpursTaskTimingFix()
    {
        constexpr char Pattern[] = "48 89 5C 24 08 57 48 83 EC 40 8B 05 ?? ?? ?? ?? 48 8B D9 0B 05 ?? ?? ?? ?? 0F 29 74 24 30 0F 29 7C 24 20 8B FA A9 00 00 00 06";
        uint8_t* update = Memory::PatternScanRange(GameText.begin, GameText.size, Pattern);
        if (!update)
            return false;
        LogAddress("spursTaskTiming", reinterpret_cast<uintptr_t>(update));

        if (MH_CreateHook(update, reinterpret_cast<LPVOID>(&SpursTaskTimingHook), reinterpret_cast<void**>(&SpursTaskTiming)) != MH_OK)
            return false;

        spdlog::info("SPURS task-step timing hook installed");
        return true;
    }

    bool InstallClothManagerTimingFix()
    {
        constexpr char Pattern[] = "48 89 74 24 10 57 48 83 EC 30 48 8B 81 80 03 00 00 41 8B F0 0F 29 74 24 20 48 8B F9 0F 28 F1 83 38 00 C7 40 04 00 00 00 00 74 ??";
        uint8_t* update = Memory::PatternScanRange(GameText.begin, GameText.size, Pattern);
        if (!update)
            return false;
        LogAddress("clothManagerUpdate", reinterpret_cast<uintptr_t>(update));

        if (MH_CreateHook(update, reinterpret_cast<LPVOID>(&ClothManagerUpdateHook), reinterpret_cast<void**>(&ClothManagerUpdate)) != MH_OK)
            return false;

        spdlog::info("Manager-backed strip-cloth preparation remains per-render");
        return true;
    }

    bool InstallClothProducerTimingFix()
    {
        constexpr char ProducerPattern[] = "48 89 5C 24 18 55 57 41 56 48 81 EC 10 01 00 00 48 8B B9 70 03 00 00 45 33 F6 41 8B E8 48 8B D9 48 8B 07 44 89 70 04 44 39 30 74 3D";
        constexpr char PublishPattern[] = "48 8B 41 08 4C 8B D1 83 38 00 C7 40 04 00 00 00 00 74 5C 49 8B 42 08 4C 63 40 04 44 3B 00 7D 4F";

        uint8_t* publisher = Memory::PatternScanRange(GameText.begin, GameText.size, PublishPattern);
        if (!publisher)
            return false;
        LogAddress("clothTransformPublish", reinterpret_cast<uintptr_t>(publisher));
        ClothTransformPublish = reinterpret_cast<ClothTransformPublishDelegate>(publisher);

        uint8_t* update = Memory::PatternScanRange(GameText.begin, GameText.size, ProducerPattern);
        if (!update)
            return false;
        LogAddress("clothProducerUpdate", reinterpret_cast<uintptr_t>(update));

        if (MH_CreateHook(update, reinterpret_cast<LPVOID>(&ClothProducerUpdateHook), reinterpret_cast<void**>(&ClothProducerUpdate)) != MH_OK)
            return false;

        spdlog::info("Installed strip-cloth producer timing fix");
        return true;
    }

    bool InstallDirectResidentClothTimingFix()
    {
        constexpr char OwnerPattern[] = "48 89 5C 24 08 57 48 81 EC B0 00 00 00 48 8B D9 0F 29 B4 24 A0 00 00 00 48 8B 89 A8 00 00 00 48 85 C9 74 ?? E8 ?? ?? ?? ?? 4C 8B 83 A0 00 00 00";
        constexpr char BonePublishPattern[] = "4C 8B DC 55 57 41 55 41 57 49 8D AB ?? ?? ?? ?? 48 81 EC ?? ?? ?? ?? 48 ?? ?? 4C 8B F9 4C 8B 89 ?? ?? ?? ?? 4C 8B A9 ?? ?? ?? ?? 48 8B B9 ?? ?? ?? ?? 48 89 85 ?? ?? ?? ?? 0F B7 81";
        constexpr char Pattern[] = "48 8B C4 55 53 48 8D A8 78 FE FF FF 48 81 EC 78 02 00 00 4C 8B 89 58 04 00 00 48 8B D9 44 0F B7 42 3C 48 89 78 18 48 8B FA 4C 89 68 E8";

        uint8_t* ownerUpdate = Memory::PatternScanRange(GameText.begin, GameText.size, OwnerPattern);
        if (!ownerUpdate)
            return false;
        LogAddress("directResidentClothOwnerUpdate", reinterpret_cast<uintptr_t>(ownerUpdate));

        uint8_t* bonePublish = Memory::PatternScanRange(GameText.begin, GameText.size, BonePublishPattern);
        if (!bonePublish)
            return false;
        LogAddress("directResidentClothBonePublish", reinterpret_cast<uintptr_t>(bonePublish));
        DirectResidentClothBonePublish = reinterpret_cast<DirectResidentClothBonePublishDelegate>(bonePublish);

        uint8_t* update = Memory::PatternScanRange(GameText.begin, GameText.size, Pattern);
        if (!update)
            return false;
        LogAddress("directResidentClothUpdate", reinterpret_cast<uintptr_t>(update));

        if (MH_CreateHook(ownerUpdate, reinterpret_cast<LPVOID>(&DirectResidentClothOwnerUpdateHook), reinterpret_cast<void**>(&DirectResidentClothOwnerUpdate)) != MH_OK)
            return false;
        if (MH_CreateHook(update, reinterpret_cast<LPVOID>(&DirectResidentClothUpdateHook), reinterpret_cast<void**>(&DirectResidentClothUpdate)) != MH_OK)
            return false;

        spdlog::info("Cloth simulation patched");
        return true;
    }

    bool InstallHairTimingFix()
    {
        constexpr char UpdatePattern[] = "40 53 48 81 EC D0 00 00 00 48 8B D9 E8 ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 8B 81 48 02 00 00 48 85 C0 74 ?? 44 8B 40 08 48 8B 91 98 02 00 00 48 8B 89 90 02 00 00 49 C1 E0 05 E8 ?? ?? ?? ??";
        uint8_t* update = Memory::PatternScanRange(GameText.begin, GameText.size, UpdatePattern);
        if (!update)
            return false;
        LogAddress("hairSimulationUpdate", reinterpret_cast<uintptr_t>(update));

        if (MH_CreateHook(update, reinterpret_cast<LPVOID>(&HairSimulationUpdateHook), reinterpret_cast<void**>(&HairSimulationUpdate)) != MH_OK)
            return false;

        spdlog::info("Bandana physics patched");
        return true;
    }

    bool InstallPhysicsWorldTimingFix()
    {
        constexpr char Pattern[] = "48 89 5C 24 10 57 48 83 EC 60 8B 41 08 45 33 C9 0F 29 74 24 50 83 E8 01 48 63 D0 0F 28 F1 0F 29 7C 24 40 48 8B D9 78 ??";
        uint8_t* update = Memory::PatternScanRange(GameText.begin, GameText.size, Pattern);
        if (!update)
            return false;
        LogAddress("physicsWorldTimeStep", reinterpret_cast<uintptr_t>(update));

        if (MH_CreateHook(update, reinterpret_cast<LPVOID>(&PhysicsWorldTimeStepHook), reinterpret_cast<void**>(&PhysicsWorldTimeStep)) != MH_OK)
            return false;

        spdlog::info("Rigid-body physics timing fix applied");
        return true;
    }

    bool InstallRagdollContactVelocityFix()
    {
        constexpr char UpdatePattern[] = "48 89 5C 24 18 55 56 41 56 48 83 EC 30 48 8B B2 40 44 00 00 4C 8B F1 48 8B AA 30 44 00 00 48 8B CA 48 8B DA E8 ?? ?? ?? ?? 48 8B CB E8 ?? ?? ?? ?? 48 8B CB E8 ?? ?? ?? ?? 84 C0 74 ?? 81 8B 04 44 00 00 00 00 40 00";
        constexpr char RadialForcePattern[] = "48 8B C4 48 89 68 10 48 89 70 18 57 48 81 EC 10 01 00 00 0F 28 05 ?? ?? ?? ?? 48 8B EA 48 8B 79 30 48 8B F1 0F 28 0D ?? ?? ?? ?? 44 0F 29 40 C8 44 0F 28 C2 0F 11 44 24 30";
        constexpr char BodyForcePattern[] = "4C 8B DC 48 81 EC B8 00 00 00 0F 28 0D ?? ?? ?? ?? 4C 8B D1 0F 28 05 ?? ?? ?? ?? 4C 8B 41 30 45 0F 29 4B B8 44 0F 28 CA 0F 11 44 24 30";
        uint8_t* update = Memory::PatternScanRange(GameText.begin, GameText.size, UpdatePattern);
        uint8_t* radialForce = Memory::PatternScanRange(GameText.begin, GameText.size, RadialForcePattern);
        uint8_t* bodyForce = Memory::PatternScanRange(GameText.begin, GameText.size, BodyForcePattern);
        if (!update || !radialForce || !bodyForce)
            return false;
        LogAddress("npcRagdollContactUpdate", reinterpret_cast<uintptr_t>(update));
        LogAddress("ragdollRadialForce", reinterpret_cast<uintptr_t>(radialForce));
        LogAddress("ragdollBodyForce", reinterpret_cast<uintptr_t>(bodyForce));

        if (MH_CreateHook(update, reinterpret_cast<LPVOID>(&NpcRagdollContactUpdateHook), reinterpret_cast<void**>(&NpcRagdollContactUpdate)) != MH_OK)
            return false;
        if (MH_CreateHook(radialForce, reinterpret_cast<LPVOID>(&RagdollRadialForceHook), reinterpret_cast<void**>(&RagdollRadialForce)) != MH_OK)
            return false;
        if (MH_CreateHook(bodyForce, reinterpret_cast<LPVOID>(&RagdollBodyForceHook), reinterpret_cast<void**>(&RagdollBodyForce)) != MH_OK)
            return false;

        spdlog::info("Ragdoll contact velocities normalized");
        return true;
    }

    bool InstallFrameRateHook()
    {
        uint8_t* target = FindTargetFps();
        if (!target)
        {
            spdlog::error("Failed to find the target FPS function");
            return false;
        }
        LogAddress("getTargetFps", reinterpret_cast<uintptr_t>(target));

        MH_STATUS status = MH_Initialize();
        if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED)
        {
            spdlog::error("Failed to initialize MinHook, status: {}", static_cast<int>(status));
            return false;
        }

        return MH_CreateHook(target, reinterpret_cast<LPVOID>(&GetTargetFpsHook), reinterpret_cast<void**>(&GetTargetFps)) == MH_OK;
    }
} // namespace

void MGS4_Initialize()
{
    spdlog::info("Starting framerate unlocker");

    if (Config.targetFramerate <= 0)
    {
        spdlog::error("Invalid target framerate: {}", Config.targetFramerate);
        return;
    }

    if (!InstallFrameRateHook())
    {
        spdlog::error("Failed to initialize the framerate unlocker");
        return;
    }

    spdlog::info("Target framerate set to {} FPS", Config.targetFramerate);

    if (!InstallSphericalCameraTimingFix())
        spdlog::error("Failed to install the spherical camera timing fix");
    if (!InstallCharacterControlTimingFix())
        spdlog::error("Failed to install the character control timing fix");
    if (!InstallGamepadVibrationTimingFix())
        spdlog::error("Failed to install the gamepad vibration timing fix");
    if (!InstallPolygonDemoTimingFix())
        spdlog::error("Failed to install the polygon-demo timing fix");
    if (!InstallMicrowaveMashTimingFix())
        spdlog::error("Failed to install the microwave mash-input timing fix");
    if (!InstallWindManagerTimingFix())
        spdlog::error("Failed to install the wind-manager timing fix");
    if (!InstallSpursTaskTimingFix())
        spdlog::error("Failed to install the SPURS task timing fix");
    if (!InstallHairTimingFix())
        spdlog::error("Failed to install the hair and articulated-chain timing fix");
    if (!InstallClothManagerTimingFix())
        spdlog::error("Failed to install the strip-cloth manager timing fix");
    if (!InstallClothProducerTimingFix())
        spdlog::error("Failed to install the strip-cloth producer timing fix");
    if (!InstallDirectResidentClothTimingFix())
        spdlog::error("Failed to install the direct resident-cloth timing fix");
    if (!InstallPhysicsWorldTimingFix())
        spdlog::error("Failed to install the physics-world timestep fix");
    if (!InstallRagdollContactVelocityFix())
        spdlog::error("Failed to install the ragdoll contact-velocity fix");

    const MH_STATUS status = MH_EnableHook(MH_ALL_HOOKS);
    if (status != MH_OK)
    {
        spdlog::error("Failed to enable one or more hooks, status: {}", static_cast<int>(status));
        MH_Uninitialize();
        return;
    }

    spdlog::info("All hooks installed successfully");
}
