#pragma once

#include "OpenKNX.h"
#include "Dali/Master.h"
#include "Dali/Commands.h"
#include "colorhelper.h"
#include "DaliHelper.h"
#include "DaliConstants.h"

#define DimmInterval 200
// ---------------------------------------------------------------------------------------------------------
// TEMPORARY DIAGNOSIS - ENABLED FOR THE v13 TEST, MUST BE REMOVED IN THE FINAL VERSION / BEFORE A PR
// Purpose: check the automatic activation mechanism on the bus. The log shows every DALI frame (incl. our own
// echo, DaliModule::loopDiagFrames), so it can be seen objectively that the temporary colour goes out WITHOUT
// ACTIVATE and the DAPC follows right behind it. If T1 fails, this separates a sequence error of the gateway
// from a driver that does not take over the temporary colour. In addition, QUERY_STATUS / QUERY_ACTUAL_LEVEL
// probes at fixed times after every RGBW command from the collection window (DaliChannel::loopDiag).
// Side effects (test only): extra frames on the bus; the level probes update the "Dimmen Status" object, the
// RGBW status (slider mode) and the "last value" with intermediate levels of the running fade. For tests of
// status objects, "switch on with last value" or bus load, comment the define out again.
// ---------------------------------------------------------------------------------------------------------
#define RGBW_DIAG
#ifdef RGBW_DIAG
#define RGBW_BUILD_TAG "v13-diag"
#else
#define RGBW_BUILD_TAG "v13"
#endif
// RGBW collection window (ms): telegrams that arrive within this time (Home Assistant: colour and brightness ~20 ms
// apart, in any order) are merged into one DALI command (temporary colour + DAPC = one fade). 0 = send at once.
// Changeable at runtime: console command "aawin".
#define RgbwWindowMs 80
// RGBW: bus time of a colour sequence in front of a DAPC (10-12 DALI frames)
#define RgbwTxMs 310
// RGBW: reserve (ms) on top of the fade time when deciding whether an own fade can still run
#define RgbwFadeReserveMs 100
// RGBW point 3 (colour alone / level alone on a lit channel), console "p3mode":
// C = like RGB, A = driver targets (GO TO LAST ACTIVE LEVEL / COPY REPORT TO TEMPORARY, DALI-2),
// B = own target with time check (any DT8 driver), auto = A for DALI-2 drivers, otherwise B
#define RgbwP3LikeRgb 0
#define RgbwP3DriverTargets 1
#define RgbwP3OwnTarget 2
#define RgbwP3Auto 255
#define DimmStatusInterval 500

class DaliChannel : public OpenKNX::Channel
{
	public:
		// RGBW: collection window (console "aawin") and the fade time of the drivers, read ONCE at start from the
		// first RGBW channel and used for all of them
		static uint16_t rgbwWindowMs;
		static uint16_t rgbwFadeMs;
		static bool rgbwFadeQueryStarted;
		// RGBW point 3 mode (console "p3mode") and the DALI version of the drivers, read ONCE at start
		static uint8_t rgbwP3Mode;
		static bool rgbwDali2;
		static bool rgbwVersionQueryStarted;
		static uint8_t rgbwEffectiveP3Mode();
        static constexpr uint16_t AllGroups = DaliConstants::AllGroups;

        DaliChannel(Dali::Master &master);
        ~DaliChannel();

		void loop() override;
		void loop1();

		// RGBW (DPT 251.600), see the concept comment in DaliChannel.cpp
		bool isRgbw();
		void rgbwOff();
		void rgbwCancel();
		void rgbwBroadcastOn();
		void rgbwBroadcastOff();

		void setup() override;
		void processInputKo(GroupObject &ko) override;
		const bool isConfigured();
		const bool isGroup();
		bool hasError();

		void init(uint8_t channelIndex, bool isGroup);
		void setOnValue(uint8_t value);
		void setGroups(uint16_t groupBits, DaliChannel* groups);
		void setDimmRef(uint8_t ref);
		void setGroupState(uint16_t group, bool state);
		void setGroupState(uint16_t group, uint8_t value);
		void setMinArc(uint8_t max);
		void setMaxArc(uint8_t max);
		void setHcl(uint8_t curve, uint16_t temp);
		void setHcl(uint8_t curve, uint8_t bri);
		uint8_t getMin();
		uint8_t getMax();
		uint16_t getGroups();

		bool isNight = false;
		const std::string name() override;
		// void writeFlash() override;
		// void readFlash(const uint8_t* data, const uint16_t size) override;
		// uint16_t flashSize() override;

	private:
        static constexpr uint8_t GroupCount = DaliConstants::GroupCount;
        static constexpr uint8_t HclDisabled = 255;
        static constexpr uint8_t NoDimmReference = 255;

        enum class DimmDirection : uint8_t {
			Down,
			Up,
			None
		};
		enum class DimmType : uint8_t {
			Brigthness,
			Color
		};
		enum class ColorChannel : uint8_t {
			Red = 0,
			Green = 1,
			Blue = 2
		};

		Dali::Master &daliMaster;

		//Relative dimming
		DimmDirection _dimmDirection = DimmDirection::None;
		uint8_t _dimmStep = 0;
		unsigned long _dimmLast = 0;
		unsigned long _dimmLastStatus = 0;
		uint8_t *currentDimmValue;
		DimmType currentDimmType;
		uint8_t _dimmStatusInterval = 0;
		uint8_t _dimmReferenceAddress = NoDimmReference;
		//Staircase light
		unsigned long startTime = 0;
		uint interval = 0;
		//Initial values
		uint8_t _min = 0;
		uint8_t _max = 0;
		uint8_t _onDay = 100;
		uint8_t _onNight = 10;
		bool _isStaircase = false;
		bool _isGroup = false;
		bool _isConfigured = false;
		//HCL
		uint8_t _hclCurve = HclDisabled;
		uint16_t _hclCurrentTemp = 0;
		uint8_t _hclCurrentBri = 0;
		bool _hclIsAlsoOn = false;
		bool _hclIsAutoMode = true;
		bool _hclLastState = true; //TODO remove
		//Reading EVG error state
		bool _getError = false;
		bool _errorState = false;
		uint32_t _errorResp = 0;
		unsigned long _lastError = 40000;
		//Current state
		bool currentState = false;
		//Current brightness
		uint8_t currentStep = 0;
		//Current lock state
		bool currentIsLocked = false;
		//Current color (R, G, B, W). With xyY the 4 bytes are used as two 16 bit values.
		uint8_t currentColor[4] = {0, 0, 0, 0};

		//Querying current state
		uint32_t _queryId = 0;
		uint16_t _queryInterval = 0;
		unsigned long _lastValueQuery = 0;

		// RGBW collection window: newest requested level (0 = none), colour changed, request came while off
		bool _winPending = false;
		unsigned long _winAt = 0;
		uint8_t _winLevel = 0;
		bool _winColour = false;
		bool _winFromOff = false;
		void rgbwRequestLevel(uint8_t arc);
		void rgbwRequestColour();
		void rgbwStartWindow();
		void rgbwFlush();
		void loopRgbwWindow();
		void loopFadeQuery();
		uint32_t _fadeQueryId = 0;
		void loopVersionQuery();
		uint32_t _versionQueryId = 0;

		// Time (0 = none) and target (DALI arc value) of the last own DAPC with a fixed target, and time of the last
		// own colour (ACTIVATE or temporary colour with DAPC). Kept in RAM, checked against the fade time only
		// (no bus query), cleared when expired.
		unsigned long _levelSentAt = 0;
		uint8_t _levelTarget = 0;
		unsigned long _colourSentAt = 0;
		uint16_t rgbwFadeWindowMs();
		bool levelFadeMayRun();
		bool colourFadeMayRun();

		// RGBW brightness from the colour value ("slider" mode, parameter rgbwBri): the largest channel of the RGBW
		// value is the brightness (255 = 100 %), the ratio of the channels is the colour mix. The driver ignores
		// the amplitude of colour values (it normalises), so the level is sent as an own level command and the
		// colour as normalised mix (_mix, largest channel 255). The colour status shows mix x level.
		uint8_t _mix[4] = {0, 0, 0, 0};
		uint8_t _lastLevelArc = 0; // last level reported through setDimmState (DALI arc value)
		bool rgbwSliderMode();
		uint8_t rgbwArcFromColour();
		void rgbwSliderStatus(uint8_t arc);

		// After the stop of a relative dimming the real level is queried once (the last UP/DOWN is still running
		// in the driver when the stop arrives, so the level queried during dimming is one step behind).
		unsigned long _stopQueryAt = 0;
		void loopStopQuery();

#ifdef RGBW_DIAG
		unsigned long _diagStart = 0;
		uint8_t _diagStep = 0;
		bool _diagActive = false;
		uint8_t _diagStatusStep = 0;
		uint32_t _diagStatusId = 0;
		void loopDiag();
		void diagStart();
#endif

		//Turn-on with last value
		// DALI arc values (not percent): 100 % and 10 % until the first level is known
		uint8_t _lastDayValue = DaliHelper::percentToArc((uint8_t)100);
		uint8_t _lastNightValue = DaliHelper::percentToArc((uint8_t)10);

		//Group membership
		uint16_t _groups = 0;

		void loopError();
		void loopDimming();
		void loopStaircase();
		void loopQueryLevel();
		uint16_t calcKoNumber(int asap);
		void setSwitchState(bool value, bool isSwitchCommand = true);
		void setDimmState(uint8_t value, bool isDimmCommand = true, bool isLastCommand = false);
		void updateCurrentDimmValue();
		void sendColor(bool activate = true); // activate = false: RGBW only, temporary colour without ACTIVATE
		void sendColorAsRGB(uint8_t r, uint8_t g, uint8_t b);
		void sendColorAsRGBW(uint8_t r, uint8_t g, uint8_t b, uint8_t w, bool activate = true);
		void sendRgbwState();
		void sendColorAsXY(uint8_t r, uint8_t g, uint8_t b);
		void sendKoStateOnChange(uint16_t koNr, const KNXValue &value, const Dpt &type);
		void setTemperature(uint16_t value);
		void setBrightness(uint8_t value);
		bool isDimmOnLocked();
		bool isDimmOffLocked();
		void queryActualLevel();
		
		void koHandleSwitch(GroupObject &ko);
		void koHandleDimmRel(GroupObject &ko);
		void koHandleDimmAbs(GroupObject &ko);
		void koHandleLock(GroupObject & ko);
		void koHandleColor(GroupObject &ko);
		void handleColorHSV(GroupObject &ko);
		void handleColorRGB(GroupObject &ko);
		void handleColorTW(GroupObject &ko);
		void handleColorXYY(GroupObject &ko);
		void handleColorRGBW(GroupObject &ko);
		void koHandleColorRel(GroupObject &ko, ColorChannel channel);
		void koHandleColorAbs(GroupObject &ko, ColorChannel channel);
		void koHandleHclCurve(GroupObject &ko);
		void koHandleScene(GroupObject &ko);

		void handleSwitchNormal(GroupObject &ko);
		void handleSwitchStaircase(GroupObject &ko);
};