#include "DaliChannel.h"
#include "RgbwHelper.h"
#include "OpenKNX.h"

DaliChannel::DaliChannel(Dali::Master &_daliMaster) : daliMaster(_daliMaster) {}

DaliChannel::~DaliChannel() {}

const std::string DaliChannel::name()
{
    if (_isStaircase)
        if (_isGroup)
            return "StaircaseChannel_G";
        else
            return "StaircaseChannel_A";

    if (_isGroup)
        return "StandardChannel_G";
    return "StandardChannel_A";
}

const bool DaliChannel::isConfigured()
{
    return _isConfigured;
}

const bool DaliChannel::isGroup()
{
    return _isGroup;
}

bool DaliChannel::hasError()
{
    return _errorState;
}

void DaliChannel::init(uint8_t channelIndex, bool ig)
{
    _channelIndex = channelIndex;
    _isGroup = ig;
}

// will be called once
// only if knx.configured == true
void DaliChannel::setup()
{
    if (_isGroup)
    {
        _isConfigured = ParamDGWG_deviceType != PT_groupType_none;
        if (!_isConfigured)
            return;

        _isStaircase = ParamDGWG_type;
        _min = 0;
        _max = 100;
        if (_isStaircase)
            interval = ParamDGWG_stairtime;
        _onDay = DaliHelper::percentToArc((float)ParamDGWG_onDay);
        _onNight = DaliHelper::percentToArc((float)ParamDGWG_onNight);
        _queryInterval = ParamDGWG_queryTime;
        _dimmStatusInterval = ParamDGWG_dimmStateInterval;
        if(ParamDGWG_hcl)
        {
            _hclCurve = ParamDGWG_hclCurve;
            _hclIsAlsoOn = ParamDGWG_hclStart;
        }
    }
    else
    {
        _isConfigured = ParamDGW_deviceType != PT_deviceType_none;
        if (!_isConfigured)
            return;

        _isStaircase = ParamDGW_type;
        _min = ParamDGW_min;
        _max = ParamDGW_max;
        if (_isStaircase)
            interval = ParamDGW_stairtime;
        _onDay = DaliHelper::percentToArc((float)ParamDGW_onDay);
        _onNight = DaliHelper::percentToArc((float)ParamDGW_onNight);
        _getError = ParamDGW_error;
        _queryInterval = ParamDGW_queryTime;
        _dimmStatusInterval = ParamDGW_dimmStateInterval;
        if(ParamDGW_hcl)
        {
            _hclCurve = ParamDGW_hclCurve;
            _hclIsAlsoOn = ParamDGW_hclStart;
        }
    }
    _min = DaliHelper::percentToArc(_min);
    _max = DaliHelper::percentToArc(_max);
    logDebugP("Min/Max %i/%i | D/N %i/%i (%.2f/%.2f) | TRH %is | Err %i | Q %is | Dimm %i", _min, _max, _onDay, _onNight, DaliHelper::arcToPercentFloat(_onDay), DaliHelper::arcToPercentFloat(_onNight), interval, _getError, _queryInterval, _dimmStatusInterval);
}

void DaliChannel::loop()
{
    if(_hclIsAutoMode != _hclLastState)
    {
        _hclLastState = _hclIsAutoMode;
        logDebugP("HCL Mode: %s", _hclIsAutoMode ? "Auto" : "Manu");
    }
}

void DaliChannel::loop1()
{
    if (!_isConfigured)
        return;

    loopStaircase();
    loopDimming();
    loopError();
    loopQueryLevel();
    loopRgbwWindow();
    loopFadeQuery();
    loopVersionQuery();
    loopStopQuery();
#ifdef RGBW_DIAG
    loopDiag();
#endif
}

#ifdef RGBW_DIAG
// Asks the driver for its real level at fixed times after an RGBW colour switch-on and logs the answers
// (the answer is logged in loopQueryLevel and also updates the "Dimmen Status" object).
void DaliChannel::loopDiag()
{
    static const uint16_t times[] = {300, 700, 1100, 1500, 2000, 3000, 5000};
    static const uint16_t statusTimes[] = {150, 500, 900, 1300, 1700, 2300, 3300};
    const uint8_t count = sizeof(times) / sizeof(times[0]);

    // answer of a QUERY_STATUS probe: bit 4 = "fade running"
    if (_diagStatusId != 0)
    {
        Dali::Response response = daliMaster.getResponse(_diagStatusId);
        if (response.state == Dali::ResponseState::NOT_REGISTERED || response.state == Dali::ResponseState::NO_ANSWER)
        {
            logDebugP("DIAG status: no answer");
            _diagStatusId = 0;
        }
        else if (response.state != Dali::ResponseState::SENT && response.state != Dali::ResponseState::WAITING)
        {
            uint8_t st = response.frame.data & 0xFF;
            logDebugP("DIAG status 0x%02X fadeRunning=%i (t+%lu ms)", st, (st >> 4) & 1, millis() - _diagStart);
            _diagStatusId = 0;
        }
    }

    if (!_diagActive)
        return;
    if (_diagStep >= count && _diagStatusStep >= count)
    {
        _diagActive = false;
        return;
    }

    unsigned long t = millis() - _diagStart;
    if (_diagStatusStep < count && _diagStatusId == 0 && _queryId == 0 && t >= statusTimes[_diagStatusStep])
    {
        _diagStatusStep++;
        if (!_isGroup)
            _diagStatusId = daliMaster.sendCommand(_channelIndex, Dali::Command::QUERY_STATUS, false, true);
        return;
    }
    if (_diagStep < count && _queryId == 0 && _diagStatusId == 0 && t >= times[_diagStep])
    {
        logDebugP("DIAG t+%lu ms: query actual level", t);
        queryActualLevel();
        _diagStep++;
    }
}

void DaliChannel::diagStart()
{
    _diagStart = millis();
    _diagStep = 0;
    _diagStatusStep = 0;
    _diagActive = true;
}
#endif

uint16_t DaliChannel::rgbwWindowMs = RgbwWindowMs;
uint16_t DaliChannel::rgbwFadeMs = 1000;
bool DaliChannel::rgbwFadeQueryStarted = false;
uint8_t DaliChannel::rgbwP3Mode = RgbwP3Auto;
bool DaliChannel::rgbwDali2 = false;
bool DaliChannel::rgbwVersionQueryStarted = false;

// ---------------------------------------------------------------------------------------------------------
// RGBW control (v13, see README sections 1.5 and 5.1):
// The driver has ONE fade engine (DA4-M, B1): every new fade command (DAPC, ACTIVATE) aborts the running fade and
// the driver stays where it is. RGBW therefore follows the RGB code (every telegram = one DALI command = one fade,
// no bus query, no waiting for the end of a fade) with these deviations:
// 1. Collection window (rgbwWindowMs, default 80 ms): telegrams that arrive close together (Home Assistant sends
//    colour and brightness ~20 ms apart, in any order) are merged. At the end of the window the colour goes out
//    as a temporary value (SET TEMPORARY RGB/WAF, no ACTIVATE) and the DAPC right behind it: the DAPC activates the
//    colour ("automatic activation"), colour and level fade together in one fade.
// 2. Switch-on (Schalten Ein, Dimmen absolut > 0) always sends the stored colour as temporary value with the DAPC.
//    Without a known colour only the DAPC goes out (like RGB).
// 3. A colour alone or a level alone on a lit channel also carries the other target, so a running fade of the
//    driver is never left half way. Selected with rgbwP3Mode (console "p3mode"):
//    A (DALI-2, default when the driver reports DALI-2): colour alone = temporary colour + GO TO LAST ACTIVE LEVEL
//      (the driver takes its own level target), level alone = COPY REPORT TO TEMPORARY + DAPC (the driver takes its
//      own colour). No time check, single channels and groups.
//    B (any DT8 driver, default for DALI-1): single channels only, time check in RAM: a colour alone while the own
//      level fade can still run = temporary colour + DAPC with the level target the gateway sent itself; a level
//      alone while the own colour fade can still run = temporary colour (gateway colour) + DAPC.
//    C (like RGB): colour alone = ACTIVATE, level alone = DAPC; an aborted fade is accepted.
// 4. Groups in mode B: a colour without level goes out with ACTIVATE (members may be on different levels).
// 5. A colour for a switched-off channel goes out with ACTIVATE like RGB, the lamp stays off and the status stays
//    "off". Exception: brightness from the RGBW value (slider mode), there a colour > 0 switches on.
// Every command that carries both targets makes an aborted fade harmless: the driver fades from where it is.
// ---------------------------------------------------------------------------------------------------------
bool DaliChannel::isRgbw()
{
    return (_isGroup ? ParamDGWG_colorType : ParamDGW_colorType) == PT_colorType_RGBW;
}

// Effective mode for point 3: A, B or C (auto = A for DALI-2 drivers, otherwise B)
uint8_t DaliChannel::rgbwEffectiveP3Mode()
{
    if (rgbwP3Mode == RgbwP3Auto)
        return rgbwDali2 ? RgbwP3DriverTargets : RgbwP3OwnTarget;
    return rgbwP3Mode;
}

// Time (ms, counted from queueing) in which a fade started by this gateway can still run in the driver:
// bus time of a colour sequence in front of it + fade time of the driver + reserve
uint16_t DaliChannel::rgbwFadeWindowMs()
{
    return RgbwTxMs + rgbwFadeMs + RgbwFadeReserveMs;
}

// A level command of this gateway (with a fixed target) was sent so recently that its fade can still run.
// An expired time is cleared, so the overflow of millis() (~49.7 days) cannot make an old value look recent.
bool DaliChannel::levelFadeMayRun()
{
    if (_levelSentAt == 0)
        return false;
    if ((unsigned long)(millis() - _levelSentAt) < rgbwFadeWindowMs())
        return true;
    _levelSentAt = 0;
    return false;
}

// A colour of this gateway was sent so recently that its fade can still run (cleared when expired, see above)
bool DaliChannel::colourFadeMayRun()
{
    if (_colourSentAt == 0)
        return false;
    if ((unsigned long)(millis() - _colourSentAt) < rgbwFadeWindowMs())
        return true;
    _colourSentAt = 0;
    return false;
}

// Level request (Schalten Ein, Dimmen absolut > 0, level from the colour value in slider mode). arc > 0.
// Call it BEFORE currentState is set by the caller: a request on a switched-off channel always takes the colour.
void DaliChannel::rgbwRequestLevel(uint8_t arc)
{
    if (!currentState)
        _winFromOff = true;
    _winLevel = arc;
    rgbwStartWindow();
}

// Colour request of a lit channel (the colour is already stored in currentColor / _mix)
void DaliChannel::rgbwRequestColour()
{
    _winColour = true;
    rgbwStartWindow();
}

void DaliChannel::rgbwStartWindow()
{
    if (rgbwWindowMs == 0)
    {
        rgbwFlush();
        return;
    }
    if (_winPending)
        return; // the window keeps running, the newest values are sent at its end
    _winPending = true;
    _winAt = millis() + rgbwWindowMs;
}

void DaliChannel::loopRgbwWindow()
{
    if (_winPending && (long)(millis() - _winAt) >= 0)
        rgbwFlush();
}

// End of the collection window: send what was requested, as one command where possible
void DaliChannel::rgbwFlush()
{
    _winPending = false;
    const bool colourKnown = !RgbwHelper::isOff(currentColor);

    const uint8_t mode = rgbwEffectiveP3Mode();

    if (_winLevel > 0)
    {
        if (colourKnown && (_winFromOff || _winColour))
        {
            // switch-on or colour + level in the window: both targets from the gateway, one fade
            sendColor(false); // temporary colour, activated by the DAPC
            daliMaster.sendArc(_channelIndex, _winLevel, _isGroup);
            logDebugP("RGBW: level %i with temporary colour (one fade)", _winLevel);
        }
        else if (!_winFromOff && mode == RgbwP3DriverTargets)
        {
            // A: level alone on a lit channel, the driver copies its own colour (report) into the temporary colour,
            // the DAPC activates it: a running colour fade goes on to its target instead of stopping half way
            daliMaster.sendExtendedCommand(_channelIndex, 0x08, Dali::ExtendedCommandDT8::COPY_REPORT_TO_TEMP, _isGroup);
            daliMaster.sendArc(_channelIndex, _winLevel, _isGroup);
            logDebugP("RGBW: level %i with the driver's own colour (copy report to temporary)", _winLevel);
        }
        else if (colourKnown && mode == RgbwP3OwnTarget && colourFadeMayRun())
        {
            // B: the own colour fade can still run, the DAPC would abort it: take the gateway colour along
            sendColor(false);
            daliMaster.sendArc(_channelIndex, _winLevel, _isGroup);
            logDebugP("RGBW: level %i with temporary colour (own colour fade may still run)", _winLevel);
        }
        else
        {
            daliMaster.sendArc(_channelIndex, _winLevel, _isGroup);
            logDebugP("RGBW: level %i", _winLevel);
        }
        _levelSentAt = millis() | 1; // never 0 (= none)
        _levelTarget = _winLevel;
    }
    else if (_winColour && colourKnown)
    {
        if (mode == RgbwP3DriverTargets)
        {
            // A: colour alone on a lit channel or group, the driver goes to its own level target (GO TO LAST ACTIVE
            // LEVEL, DALI-2) and activates the temporary colour with it: one fade, a running level fade is not lost
            sendColor(false);
            daliMaster.sendCommand(_channelIndex, Dali::Command::GO_TO_LAST, _isGroup);
            logDebugP("RGBW: colour with the driver's own level (go to last active level)");
        }
        else if (mode == RgbwP3OwnTarget && !_isGroup && levelFadeMayRun())
        {
            // B: the own level fade can still run, ACTIVATE would abort it. Send the colour with the level target
            // the gateway sent itself, the driver fades from where it is to both targets.
            sendColor(false);
            daliMaster.sendArc(_channelIndex, _levelTarget, _isGroup);
            _levelSentAt = millis() | 1;
            logDebugP("RGBW: colour with the running level target %i (one fade)", _levelTarget);
        }
        else
        {
            sendColor(true);
            logDebugP("RGBW: colour (ACTIVATE)");
        }
    }

    _winLevel = 0;
    _winColour = false;
    _winFromOff = false;
#ifdef RGBW_DIAG
    diagStart();
#endif
}

// Drops a pending window and the level target (scene, lock, group or broadcast command, relative dimming)
void DaliChannel::rgbwCancel()
{
    _winPending = false;
    _winLevel = 0;
    _winColour = false;
    _winFromOff = false;
    _levelSentAt = 0;
}

// Plain level off; the colour stays stored in gateway and driver
void DaliChannel::rgbwOff()
{
    rgbwCancel();
    currentState = false;
    logDebugP("RGBW: off");
    daliMaster.sendArc(_channelIndex, 0x00, _isGroup);
}

// Broadcast on: DAPC 254 went out to everybody already. A switched-off RGBW channel with a known colour gets its
// colour together with the same level (temporary colour + DAPC): the driver fades to both targets.
void DaliChannel::rgbwBroadcastOn()
{
    rgbwCancel();
    if (!currentState && !RgbwHelper::isOff(currentColor))
    {
        sendColor(false);
        daliMaster.sendArc(_channelIndex, 0xFE, _isGroup);
    }
    currentState = true;
}

// Broadcast off: DAPC 0 went out to everybody, only the state is updated here
void DaliChannel::rgbwBroadcastOff()
{
    rgbwCancel();
    currentState = false;
}

// Reads the fade time ONCE for all RGBW channels (QUERY_FADE_SPEEDS at the first RGBW channel or group with a
// reference address; high nibble = fade time, low = fade rate). All RGBW drivers of an installation are expected
// to have the same fade time (README). Without an answer the default of 1000 ms stays.
void DaliChannel::loopFadeQuery()
{
    static const uint32_t fadeTimes[16] = {0, 700, 1000, 1400, 2000, 2800, 4000, 5700, 8000, 11300, 16000, 22600, 32000, 45300, 64000, 90500};

    if (_fadeQueryId == 0)
    {
        if (rgbwFadeQueryStarted || !isRgbw())
            return;
        if (_isGroup && _dimmReferenceAddress == NoDimmReference)
            return;
        rgbwFadeQueryStarted = true;
        _fadeQueryId = daliMaster.sendCommand(_isGroup ? _dimmReferenceAddress : _channelIndex, Dali::Command::QUERY_FADE_SPEEDS, false, true);
        if (_fadeQueryId == 0)
            logDebugP("RGBW: fade time query could not be sent, using %i ms", rgbwFadeMs);
        return;
    }

    Dali::Response response = daliMaster.getResponse(_fadeQueryId);
    if (response.state == Dali::ResponseState::SENT || response.state == Dali::ResponseState::WAITING)
        return;
    _fadeQueryId = 0;
    if (response.state == Dali::ResponseState::NOT_REGISTERED || response.state == Dali::ResponseState::NO_ANSWER ||
        (response.frame.flags & DALI_FRAME_ERROR))
    {
        logDebugP("RGBW: fade time query failed, using %i ms", rgbwFadeMs);
        return;
    }
    uint8_t raw = response.frame.data & 0xFF;
    uint32_t fade = fadeTimes[raw >> 4];
    rgbwFadeMs = fade > 65000 ? 65000 : fade; // 16 bit
    logInfoP("RGBW: fade time x=%i = %i ms (raw 0x%02X), used for all RGBW channels", raw >> 4, rgbwFadeMs, raw);
}

// Reads the DALI version ONCE (QUERY VERSION NUMBER at the first RGBW channel or group with a reference address).
// DALI-2 answers 0x08 or higher (version 2.0 = 8). Decides the automatic mode of point 3 (A for DALI-2, otherwise B)
// for all RGBW channels. Without an answer the driver counts as DALI-1.
void DaliChannel::loopVersionQuery()
{
    if (_versionQueryId == 0)
    {
        if (rgbwVersionQueryStarted || !isRgbw())
            return;
        if (_isGroup && _dimmReferenceAddress == NoDimmReference)
            return;
        rgbwVersionQueryStarted = true;
        _versionQueryId = daliMaster.sendCommand(_isGroup ? _dimmReferenceAddress : _channelIndex, Dali::Command::QUERY_VERSION, false, true);
        if (_versionQueryId == 0)
            logDebugP("RGBW: version query could not be sent, treated as DALI-1");
        return;
    }

    Dali::Response response = daliMaster.getResponse(_versionQueryId);
    if (response.state == Dali::ResponseState::SENT || response.state == Dali::ResponseState::WAITING)
        return;
    _versionQueryId = 0;
    if (response.state == Dali::ResponseState::NOT_REGISTERED || response.state == Dali::ResponseState::NO_ANSWER ||
        (response.frame.flags & DALI_FRAME_ERROR))
    {
        logInfoP("RGBW: version query failed, treated as DALI-1 (point 3 mode B)");
        return;
    }
    uint8_t raw = response.frame.data & 0xFF;
    rgbwDali2 = raw >= 0x08;
    logInfoP("RGBW: DALI version 0x%02X = %s, point 3 mode %s", raw, rgbwDali2 ? "DALI-2" : "DALI-1",
             rgbwEffectiveP3Mode() == RgbwP3DriverTargets ? "A (driver targets)" : (rgbwEffectiveP3Mode() == RgbwP3OwnTarget ? "B (own target)" : "C (like RGB)"));
}

// One level query a short time after the stop of a relative dimming, so that the status objects show the level
// the driver really reached (not the one before the last UP/DOWN step).
void DaliChannel::loopStopQuery()
{
    if (_stopQueryAt == 0 || (long)(millis() - _stopQueryAt) < 0)
        return;
    if (_queryId != 0)
        return; // an answer is still on its way, ask afterwards
    _stopQueryAt = 0;
    // dimming started again, or the channel is off for us (a late answer must not switch the status on again)
    if (_dimmDirection != DimmDirection::None || !currentState)
        return;
    logDebugP("Query actual level after the dimming stop");
    queryActualLevel();
}

void DaliChannel::loopStaircase()
{
    if (_isStaircase && currentState)
    {
        if (millis() - startTime > interval * 1000)
        {
            logDebugP("Time expired");
            currentState = false;
            daliMaster.sendArc(_channelIndex, 0x00, _isGroup);
            setSwitchState(false);
        }
    }
}

void DaliChannel::loopDimming()
{
    if (_dimmDirection != DimmDirection::None)
    {
        // RGBW: like RGB, no wait. An UP/DOWN right after a colour change can abort the colour fade (accepted).
        if(millis() - _dimmLast > DimmInterval)
        {
            if (_dimmDirection == DimmDirection::Up)
            {
                if (currentDimmType == DimmType::Brigthness)
                {
                    queryActualLevel();
                    daliMaster.sendCommand(_channelIndex, Dali::Command::UP, _isGroup);
                }

                *currentDimmValue = *currentDimmValue + 1;
                if (*currentDimmValue == 254)
                {
                    logDebugP("Dimm Stop at 254");
                    _dimmDirection = DimmDirection::None;
                    updateCurrentDimmValue();
                }else if (*currentDimmValue >= _max)
                {
                    logDebugP("Dimm Stop at max %i (%i)", _max, *currentDimmValue);
                    _dimmDirection = DimmDirection::None;
                    updateCurrentDimmValue();
                }
            }
            else if (_dimmDirection == DimmDirection::Down)
            {
                // RGBW (DA4-M): the DOWN that follows the OFF keeps the driver lit at its minimum (it only went out
                // when the dimming ended), and the loop kept repeating "DOWN, OFF" every 200 ms until the stop
                // telegram came: lamp at minimum, bus flooded with status telegrams. So the step that reaches the
                // minimum sends no further DOWN, switches off and ends the dimming.
                bool rgbwDown = isRgbw() && !_isStaircase && currentDimmType == DimmType::Brigthness;
                if (rgbwDown && *currentDimmValue == 0)
                {
                    // already off: nothing to dim down
                    _dimmDirection = DimmDirection::None;
                    _dimmLast = millis();
                    return;
                }
                bool lastStep = rgbwDown && !isDimmOffLocked() && ((int)*currentDimmValue - 1 <= (int)_min || *currentDimmValue <= 1);

                if (currentDimmType == DimmType::Brigthness && !lastStep)
                {
                    queryActualLevel();
                    daliMaster.sendCommand(_channelIndex, Dali::Command::DOWN, _isGroup);
                }

                *currentDimmValue = *currentDimmValue - 1;
                if (*currentDimmValue <= _min || *currentDimmValue == 0) {
                    logDebugP("Dimm Stop at: %i", *currentDimmValue);
                    if (!lastStep) // lastStep: the status follows below, as "off" (not as the minimum level, "on")
                        updateCurrentDimmValue();
                    if (isDimmOffLocked()) {
                        logDebugP("Stop here because Dimm off is locked!");
                        _dimmDirection = DimmDirection::None;
                    } else {
                        logDebugP("Turn off device");
                        if (isRgbw())
                        {
                            rgbwOff();
                            if (rgbwDown)
                            {
                                _dimmDirection = DimmDirection::None;
                                _queryId = 0; // an answer still on its way (level before the OFF) must not overwrite the status
                                *currentDimmValue = 0;
                                setDimmState(0, true, true); // status objects: off, 0 %
                            }
                        }
                        else
                            daliMaster.sendCommand(_channelIndex, Dali::Command::OFF, _isGroup);
                    }
                }
            }
            
            _dimmLast = millis();
        }

        if(_dimmStatusInterval != 0 && millis() - _dimmLastStatus > (_dimmStatusInterval*100))
        {
            _dimmLastStatus = millis();
            updateCurrentDimmValue();
        }
    }
}

void DaliChannel::loopError()
{
    if (!_isGroup && _getError)
    {
        if (millis() - _lastError > 60000)
        {
            _errorResp = daliMaster.sendCommand(_channelIndex, Dali::Command::QUERY_STATUS, false, true);
            _lastError = millis();
            logDebugP("Querying EVG %i", _errorResp);
            return;
        }
        if(_errorResp == 0)
            return; // we did not send a query

        Dali::Response response = daliMaster.getResponse(_errorResp);
        if(response.state == Dali::ResponseState::NOT_REGISTERED)
        {
            // There is no response we can wait for
            _errorResp = 0;
            return;
        }
        if(response.state == Dali::ResponseState::SENT || response.state == Dali::ResponseState::WAITING)
            return; // no answer yet
        
        if(response.state == Dali::ResponseState::NO_ANSWER)
        {
            logErrorP("EVG did not respond");
            _errorState = true;
        }
        else
        {
            logDebugP("EVG responded");
            _errorState = false;
        }

        bool val = knx.getGroupObject(calcKoNumber(DGW_Koerror)).value(Dpt(1, 1));
        _errorState = val != 0;
        if (val != _errorState)
            knx.getGroupObject(calcKoNumber(DGW_Koerror)).value((val != 0), DPT_Switch);

        if(_errorState)
        {
            logErrorP("EVG reports an error");
        }
        _errorResp = 0;
    }
}

void DaliChannel::loopQueryLevel()
{
    if(_queryInterval > 0 && _queryId == 0 && (millis() - _lastValueQuery) > (_queryInterval*1000))
    {
        logDebugP("Query actual level");
        _lastValueQuery = millis();
        if(_lastValueQuery == 0) _lastValueQuery++;

        queryActualLevel();
        logDebugP("id: %i", _queryId);
        return;
    }
    if(_queryId != 0)
    {
        Dali::Response response = daliMaster.getResponse(_queryId);
        if(response.state == Dali::ResponseState::NOT_REGISTERED)
        {
            // There is no response we can wait for
            _queryId = 0;
            return;
        }
        if(response.state == Dali::ResponseState::SENT || response.state == Dali::ResponseState::WAITING)
            return; // no answer yet
        if(response.state == Dali::ResponseState::NO_ANSWER)
        {
            logErrorP("EVG did not respond");
            _queryId = 0;
            return;
        }
        else
        {
            _queryId = 0;
            if(response.frame.flags & DALI_FRAME_ERROR)
            {
                logErrorP("EVG reports an error");
                return;
            } else {
                uint8_t data = response.frame.data & 0xFF;
#ifdef RGBW_DIAG
                logDebugP("Got new actual level %i%%-%i (t+%lu ms)", DaliHelper::arcToPercent(data), data, millis() - _diagStart);
#else
                logDebugP("Got new actual level %i%%-%i", DaliHelper::arcToPercent(data), data);
#endif
                setDimmState(data, false, true);

                if(currentDimmType == DimmType::Brigthness)
                    *currentDimmValue = data;
            }
        }
    }
}

uint16_t DaliChannel::calcKoNumber(int asap)
{
    if (_isGroup)
        return asap + (DGWG_KoBlockSize * _channelIndex) + DGWG_KoOffset;

    return asap + (DGW_KoBlockSize * _channelIndex) + DGW_KoOffset;
}

void DaliChannel::processInputKo(GroupObject &ko)
{
    int chanIndex = 0;
    if (_isGroup)
    {
        chanIndex = (ko.asap() - DGWG_KoOffset) % DGWG_KoBlockSize;
        //logDebugP("Got GROUP KO %i", chanIndex);
    }
    else
    {
        chanIndex = (ko.asap() - DGW_KoOffset) % DGW_KoBlockSize;
        //logDebugP("Got SHORT KO %i", chanIndex);
    }

    // Case numbers not listed below (1, 4, 7, 10, 13, 16, 19) are the *_state KOs; they are handed to us
    // when we write them ourselves and are ignored here.
    switch (chanIndex)
    {
    case DGW_Koswitch:
        koHandleSwitch(ko);
        break;

    case DGW_Kodimm_relative:
        koHandleDimmRel(ko);
        break;

    case DGW_Kodimm_absolute:
        koHandleDimmAbs(ko);
        break;

    case DGW_Kolock:
        koHandleLock(ko);
        break;

    case DGW_Kocolor:
        koHandleColor(ko);
        break;

    case DGW_Kocolor_red_relative:
        koHandleColorRel(ko, ColorChannel::Red);
        break;

    case DGW_Kocolor_red_absolute:
        koHandleColorAbs(ko, ColorChannel::Red);
        break;

    case DGW_Kocolor_green_relative:
        koHandleColorRel(ko, ColorChannel::Green);
        break;

    case DGW_Kocolor_green_absolute:
        koHandleColorAbs(ko, ColorChannel::Green);
        break;

    case DGW_Kocolor_blue_relative:
        koHandleColorRel(ko, ColorChannel::Blue);
        break;

    case DGW_Kocolor_blue_absolute:
        koHandleColorAbs(ko, ColorChannel::Blue);
        break;

    case DGW_Kohcl_curve:
        koHandleHclCurve(ko);
        break;

    case DGW_Koscene:
        koHandleScene(ko);
        break;
    }
}

void DaliChannel::koHandleHclCurve(GroupObject &ko)
{
    uint8_t curve = ko.value(Dpt(5,1));
    if(curve > 2)
    {
        logErrorP("Ignoring HCL curve, too high: %i, max %i", curve, 2);
        return;
    }
    _hclCurve = curve;
}

void DaliChannel::koHandleScene(GroupObject &ko)
{
    uint8_t number = ko.value(Dpt(17,1));
    logDebugP("Scene KNX %i to DALI %i", number + 1, number);
    if(number > 15)
    {
        logErrorP("Ignoring scene, too high: %i, max 15", number);
        return;
    }

    // the driver executes the scene on its own: a pending RGBW window and the level target are dropped
    if (isRgbw())
        rgbwCancel();
    daliMaster.sendCommand(_channelIndex, Dali::Command::GO_TO_SCENE | number, _isGroup);
}

void DaliChannel::koHandleColorRel(GroupObject &ko, ColorChannel channel)
{
    logDebugP("Color relative %i", (int)channel);
    if (currentIsLocked)
    {
        logErrorP("is locked");
        return;
    }

    uint8_t dimmLock = _isGroup ? ParamDGWG_dimmLock : ParamDGW_dimmLock;
    if(dimmLock == PT_dimmLock_noBoth || dimmLock == PT_dimmLock_noOn)
    {
        logDebugP("ignored due settings");
        return;
    }

    if(_isGroup ? ParamDGWG_hcl_manu_col : ParamDGW_hcl_manu_col)
        _hclIsAutoMode = false;

    _dimmStep = ko.value(Dpt(3, 7, 1));

    if (_dimmStep == 0)
    {
        logDebugP("Dimm Stop");
        _dimmDirection = DimmDirection::None;
        _dimmLast = 0;
        updateCurrentDimmValue();
        return;
    }

    currentDimmValue = &currentStep;
    currentDimmType = DimmType::Color;
    _dimmLastStatus = millis();
    _dimmDirection = ko.value(Dpt(3, 7, 0)) ? DimmDirection::Up : DimmDirection::Down;
    if (_dimmDirection == DimmDirection::Up)
    {
        logDebugP("Dimm Up Start %i/%i", currentStep, *currentDimmValue);
    }
    else if (_dimmDirection == DimmDirection::Down)
    {
        logDebugP("Dimm Down Start %i/%i", currentStep, *currentDimmValue);
    }
}

void DaliChannel::koHandleColorAbs(GroupObject &ko, ColorChannel channel)
{
    logDebugP("Color absolute %i", (int)channel);
    if (currentIsLocked)
    {
        logErrorP("is locked");
        return;
    }

    if(_isGroup ? ParamDGW_hcl_manu_col : ParamDGW_hcl_manu_col)
        _hclIsAutoMode = false;
        
    logDebugP("AutoSwitchConfig %i %i", ParamDGW_hcl_manu_col, _hclIsAutoMode);

    currentColor[(uint8_t)channel] = ko.value(Dpt(5, 4));
    updateCurrentDimmValue();
    sendColor();
    logDebugP("AutoSwitchConfig2 %i %i", ParamDGW_hcl_manu_col, _hclIsAutoMode);
}

void DaliChannel::koHandleSwitch(GroupObject &ko)
{
    logDebugP("Switching");
    if (currentIsLocked)
    {
        logErrorP("is locked");
        return;
    }

    if (_isStaircase)
        handleSwitchStaircase(ko);
    else
        handleSwitchNormal(ko);
}

void DaliChannel::handleSwitchNormal(GroupObject &ko)
{
    bool value = ko.value(DPT_Switch);
    if (value)
    {
        uint8_t onValue = isNight ? _onNight : _onDay;
        if (onValue == 0)
            onValue = isNight ? _lastNightValue : _lastDayValue;
    
        bool hclBriActivated = false;
        if(_hclCurve != HclDisabled && _hclIsAutoMode)
        {
            uint8_t _channelIndex = _hclCurve;
            hclBriActivated = ParamDGWH_checkBrightness;
            if(hclBriActivated)
            {
                onValue = DaliHelper::percentToArc(_hclCurrentBri);
                logDebugP("Turning on HCL");
            }
        }
        if(!hclBriActivated) {
            logDebugP(isNight ? "Turning on night value" : "Turning on day value");
        }
        if (isRgbw())
            rgbwRequestLevel(onValue); // collection window, then (temporary colour +) DAPC
        else
            daliMaster.sendArc(_channelIndex, onValue, _isGroup);

        bool hclTempActivated = false;
        if(_hclCurve != HclDisabled && _hclIsAutoMode)
        {
            uint8_t _channelIndex = _hclCurve;
            hclTempActivated = ParamDGWH_checkTemperature;
            if(hclTempActivated)
            {
                logDebugP("Setting temperature to %iK", _hclCurrentTemp);
                setTemperature(_hclCurrentTemp);
            }
        }
        setDimmState(onValue, true);
    }
    else
    {
        logDebugP("Turning off");
        if (isRgbw())
            rgbwOff();
        else
            daliMaster.sendArc(_channelIndex, 0x00, _isGroup);
        setSwitchState(false);
    }
    currentState = value;
}

void DaliChannel::handleSwitchStaircase(GroupObject &ko)
{
    if (ko.value(DPT_Switch))
    {
        if (currentState)
        {
            logDebugP("already on");

            bool retrigger = _isGroup ? ParamDGWG_manuoff : ParamDGW_manuoff;
            if (retrigger)
            {
                logDebugP("retriggered");
                startTime = millis();
                return;
            }
            return;
        }
        currentState = true;
        startTime = millis();
        logDebugP("interval %i", interval);

        uint8_t onValue = isNight ? _onNight : _onDay;
        if (onValue == 0)
            onValue = isNight ? _lastNightValue : _lastDayValue;
        if(_hclCurve != HclDisabled && _hclIsAutoMode)
        {
            onValue = DaliHelper::percentToArc(_hclCurrentBri);
            logDebugP("Turning on HCL");
        } else {
            logDebugP(isNight ? "Turning on night value" : "Turning on day value");
        }
        daliMaster.sendArc(_channelIndex, onValue, _isGroup);
        if(_hclCurve != HclDisabled && _hclIsAutoMode)
            setTemperature(_hclCurrentTemp);
        setDimmState(DaliHelper::percentToArc(onValue));
    }
    else
    {
        bool manuOff = _isGroup ? ParamDGWG_manuoff : ParamDGW_manuoff;
        if (!manuOff)
        {
            logErrorP("no manuel off");
            return;
        }

        logDebugP("Turning off");
        daliMaster.sendArc(_channelIndex, 0x00, _isGroup);
        setSwitchState(false);
        currentState = false;
    }
}

void DaliChannel::koHandleDimmRel(GroupObject &ko)
{
    logDebugP("Dimming relative");
    if (currentIsLocked)
    {
        logErrorP("is locked");
        return;
    }    

    if(_isGroup ? ParamDGWG_hcl_manu_bri : ParamDGW_hcl_manu_bri)
        _hclIsAutoMode = false;

    _dimmStep = ko.value(Dpt(3, 7, 1));

    if (_dimmStep == 0)
    {
        logDebugP("Dimm Stop");
        _dimmDirection = DimmDirection::None;
        _dimmLast = 0;
        updateCurrentDimmValue();
        // the last UP/DOWN is still running in the driver: ask for the final level shortly after
        _stopQueryAt = (millis() + 300) | 1; // never 0 (= none)
        return;
    }

    if (isRgbw() && !_isStaircase)
    {
        // a pending window goes out first; UP/DOWN has no fixed level target (point 3 does not apply afterwards)
        if (_winPending)
            rgbwFlush();
        _levelSentAt = 0;
    }

    currentDimmValue = &currentStep;
    currentDimmType = DimmType::Brigthness;
    _dimmLastStatus = millis();
    _dimmDirection = ko.value(Dpt(3, 7, 0)) ? DimmDirection::Up : DimmDirection::Down;
    if (_dimmDirection == DimmDirection::Up)
    {
        if(!currentState)
        {
            if (isDimmOnLocked())
            {
                logDebugP("ignored because Dimm On is locked!");
                _dimmDirection = DimmDirection::None;
                _dimmLast = 0;
            }
            else 
            {
                daliMaster.sendCommand(_channelIndex, Dali::Command::RECALL_MIN, _isGroup, true);
                *currentDimmValue = _min;
                currentState = true;
                queryActualLevel();
                logDebugP("starting with min %i", _min);
                _dimmLast = millis();
            }
        }
        logDebugP("Dimm Up Start %i/%i", currentStep, *currentDimmValue);
    }
    else if (_dimmDirection == DimmDirection::Down)
    {
        logDebugP("Dimm Down Start %i/%i", currentStep, *currentDimmValue);
    }
}

void DaliChannel::koHandleDimmAbs(GroupObject &ko)
{
    logDebugP("Dimming absolute");
    if (currentIsLocked)
    {
        logErrorP("is locked");
        return;
    }

    if(_isGroup ? ParamDGWG_hcl_manu_bri : ParamDGW_hcl_manu_bri)
        _hclIsAutoMode = false;

    uint8_t value = ko.value(Dpt(5, 1));
    logDebugP("Dimming absolute to %i%%", value);
    uint8_t arc = DaliHelper::percentToArc(value);
    if (isRgbw() && !_isStaircase)
    {
        if (arc == 0)
            rgbwOff();
        else
            rgbwRequestLevel(arc); // collection window, then (temporary colour +) DAPC
    }
    else
        daliMaster.sendArc(_channelIndex, arc, _isGroup);
    setDimmState(arc, true, true);
    // keep the on/off flag in sync, otherwise e.g. a colour telegram after "dimm to 0" is ignored
    if (!_isStaircase)
        currentState = arc > 0;
}

bool DaliChannel::rgbwSliderMode()
{
    return isRgbw() && !_isStaircase && (_isGroup ? ParamDGWG_rgbwBri : ParamDGW_rgbwBri) == PT_rgbwBri_colour;
}

// DALI arc value that belongs to the amplitude (largest channel) of the stored colour
uint8_t DaliChannel::rgbwArcFromColour()
{
    return DaliHelper::percentToArc(RgbwHelper::percentFromAmplitude(RgbwHelper::maxChannel(currentColor)));
}

// Slider mode: the level changed (absolute / relative dimming, switch on, query of the driver). The colour status
// follows as mix x level, so that a client that reads the brightness from the colour value sees it.
void DaliChannel::rgbwSliderStatus(uint8_t arc)
{
    if (arc == 0 || !rgbwSliderMode() || RgbwHelper::isOff(_mix) || RgbwHelper::isOff(currentColor))
        return;
    if (rgbwArcFromColour() == arc)
        return; // the colour value already stands for this level (e.g. it just came in)
    uint8_t amp = RgbwHelper::amplitudeFromPercent(DaliHelper::arcToPercentFloat(arc));
    if (amp == 0)
        amp = 1;
    RgbwHelper::scale(_mix, amp, currentColor);
    sendRgbwState();
}

void DaliChannel::koHandleLock(GroupObject &ko)
{
    bool value = ko.value(Dpt(1, 1));

    if (_isGroup)
    {
        if (ParamDGWG_locknegate)
            value = !value;
    }
    else
    {
        if (ParamDGW_locknegate)
            value = !value;
    }

    if (currentIsLocked == value)
        return;
    currentIsLocked = value;
    uint8_t behave;
    uint8_t behavevalue;
    if (currentIsLocked)
    {
        logDebugP("Locking");
        behave = ParamDGW_lockbehave;
        behavevalue = ParamDGW_lockvalue;
    }
    else
    {
        logDebugP("Unlocking");
        behave = ParamDGW_unlockbehave;
        behavevalue = ParamDGW_unlockvalue;
    }

    switch (behave)
    {
    // nothing
    case PT_lock_no:
        logDebugP("Nothing");
        return;

    // Turn off
    case PT_lock_off:
    {
        logDebugP("On");
        behavevalue = isNight ? _onNight : _onDay;
        break;
    }

    // Turn on
    case PT_lock_on:
    {
        logDebugP("Off");
        behavevalue = 0;
        break;
    }

    // Fixed value
    case PT_lock_value:
    {
        logDebugP("Value");
        break;
    }
    }
    logDebugP("%i - %i", behavevalue, DaliHelper::percentToArc(behavevalue));
    if (isRgbw())
        rgbwCancel();
    daliMaster.sendArc(_channelIndex, behavevalue, _isGroup);
    setDimmState(DaliHelper::percentToArc(behavevalue));
}

void DaliChannel::koHandleColor(GroupObject &ko)
{
    logDebugP("Color absolute [%s]", RGBW_BUILD_TAG);
    if (currentIsLocked)
    {
        logErrorP("is locked");
        return;
    }

    if(_isGroup ? ParamDGWG_hcl_manu_col : ParamDGW_hcl_manu_col)
        _hclIsAutoMode = false;

    logDebugP("AutoConf %i %i %i", _isGroup, _isGroup ? ParamDGWG_hcl_manu_col : ParamDGW_hcl_manu_col, _hclIsAutoMode);

    uint8_t colorType = _isGroup ? ParamDGWG_colorType : ParamDGW_colorType;

    switch(colorType)
    {
        case PT_colorType_HSV:
            handleColorHSV(ko);
            break;

        case PT_colorType_RGB:
            handleColorRGB(ko);
            break;

        case PT_colorType_TW:
            handleColorTW(ko);
            break;

        case PT_colorType_XYY:
            handleColorXYY(ko);
            break;

        case PT_colorType_RGBW:
            handleColorRGBW(ko); // switching and status objects are handled there
            return;
    }

    if(!currentState)
        setDimmState(_onDay, true, true); // TODO get real

    logDebugP("AutoConf %i %i %i", _isGroup, _isGroup ? ParamDGW_hcl_manu_col : ParamDGWG_hcl_manu_col, _hclIsAutoMode);
}

void DaliChannel::handleColorHSV(GroupObject &ko)
{
    uint32_t value = ko.value(Dpt(232, 600));
    logDebugP("Got Color: %X", value);

    currentColor[0] = (value >> 16) & 0xFF;
    currentColor[1] = (value >> 8) & 0xFF;
    currentColor[2] = value & 0xFF;

    sendKoStateOnChange(DGW_Kocolor_rgb_state, value, Dpt(232, 600));
    sendKoStateOnChange(DGW_Kocolor_red_state, currentColor[0], Dpt(5, 4));
    sendKoStateOnChange(DGW_Kocolor_green_state, currentColor[1], Dpt(5, 4));
    sendKoStateOnChange(DGW_Kodimm_state, currentColor[2], Dpt(5, 4));

    sendColor();
}

void DaliChannel::handleColorRGB(GroupObject &ko)
{
    uint32_t value = ko.value(Dpt(232, 600));
    logDebugP("Got Color: %X", value);

    currentColor[0] = (value >> 16) & 0xFF;
    currentColor[1] = (value >> 8) & 0xFF;
    currentColor[2] = value & 0xFF;

    sendKoStateOnChange(DGW_Kocolor_rgb_state, value, Dpt(232, 600));
    sendKoStateOnChange(DGW_Kocolor_red_state, currentColor[0], Dpt(5, 4));
    sendKoStateOnChange(DGW_Kocolor_green_state, currentColor[1], Dpt(5, 4));
    sendKoStateOnChange(DGW_Kocolor_blue_state, currentColor[2], Dpt(5, 4));

    sendColor();
}

void DaliChannel::handleColorRGBW(GroupObject &ko)
{
    // DPT 251.600 has 6 bytes: R, G, B, W, reserved, validity bits (mR, mG, mB, mW)
    if (ko.valueSize() != RgbwHelper::DataLength)
    {
        logErrorP("RGBW: unexpected object size %i", (int)ko.valueSize());
        return;
    }

    // Decode into a copy first: all channels 0 means "off" and must not overwrite the stored colour.
    // A single channel keeps the channels that are not marked valid (the colour of the driver is known). A group
    // always takes a complete colour: its members can also belong to other groups or be changed on their own, so
    // the colour stored in the group is not reliable. The channels that are not marked valid are 0 there.
    uint8_t newColor[4] = {0, 0, 0, 0};
    if (!_isGroup)
        memcpy(newColor, currentColor, sizeof(newColor));
    const uint8_t valid = RgbwHelper::decode(ko.valueRef(), newColor);
    if (valid == 0)
    {
        logDebugP("RGBW: no valid channel in telegram, ignored");
        return;
    }

    logDebugP("Got RGBW: %i %i %i %i (valid %X)%s", newColor[0], newColor[1], newColor[2], newColor[3], valid, _isGroup && valid != 0x0F ? " group: channels that are not valid are 0" : "");

    if (RgbwHelper::isOff(newColor))
    {
        // R = G = B = W = 0 -> switch the channel off (status objects report "Off", 0 %).
        // The last colour stays stored in gateway and driver, so "Schalten = Ein" or the next
        // colour telegram brings the light back with that colour instead of staying dark.
        logDebugP("RGBW: all channels 0 -> switching off");
        rgbwOff();
        setDimmState(0, true, true);
        currentState = false;
        return;
    }

    memcpy(currentColor, newColor, sizeof(currentColor));
    const bool slider = rgbwSliderMode();
    if (slider)
        RgbwHelper::normalise(currentColor, _mix);
    sendRgbwState();

    if (slider)
    {
        // Brightness from the RGBW value: the largest channel is the level, so a colour > 0 switches on (from off
        // with the colour, see rgbwRequestLevel) and always goes out together with its level.
        uint8_t sliderArc = rgbwArcFromColour();
        if (sliderArc == 0)
            sliderArc = 1; // a colour > 0 is "on", the smallest level at least
        logDebugP("RGBW: brightness from the colour value: %i %%, level %i", (int)(RgbwHelper::percentFromAmplitude(RgbwHelper::maxChannel(currentColor)) + 0.5f), sliderArc);
        _winColour = true;
        rgbwRequestLevel(sliderArc);
        setDimmState(sliderArc, true, true);
        currentState = true;
    }
    else if (currentState)
    {
        // lit channel: colour alone, through the collection window (merged with a level that follows)
        rgbwRequestColour();
    }
    else
    {
        // switched-off channel: like RGB, the colour goes to the driver with ACTIVATE (fades unseen at level 0),
        // the lamp stays off and the status stays "off". The next switch-on sends the colour again with its DAPC.
        logDebugP("RGBW: colour for a switched-off channel, lamp stays off");
        sendColor(true);
    }
}

void DaliChannel::sendRgbwState()
{
    GroupObject &ko = knx.getGroupObject(calcKoNumber(DGW_Kocolor_rgb_state));
    if (ko.valueSize() != RgbwHelper::DataLength)
        return;

    uint8_t data[RgbwHelper::DataLength];
    RgbwHelper::encode(currentColor, data);

    // sendKoStateOnChange() has no DPT for 6 byte colour objects -> compare and write raw
    uint8_t *state = ko.valueRef();
    if (memcmp(state, data, RgbwHelper::DataLength) != 0)
    {
        memcpy(state, data, RgbwHelper::DataLength);
        ko.objectWritten();
    }
}

void DaliChannel::handleColorTW(GroupObject &ko)
{
    uint16_t kelvin = ko.value(Dpt(7, 600));
    setTemperature(kelvin);
}

void DaliChannel::handleColorXYY(GroupObject &ko)
{
    uint8_t* data = ko.valueRef();
    /*
        <UnsignedInteger Id="DPST-242-600_F-1" Width="16" Name="x-axis" Unit="None" />
        <UnsignedInteger Id="DPST-242-600_F-2" Width="16" Name="y-axis" Unit="None" />
        <UnsignedInteger Id="DPST-242-600_F-3" Width="8" Name="brightness" Unit="%" />
        <Reserved Width="6" />
        <Bit Id="DPST-242-600_F-10" Cleared="invalid" Set="valid" Name="Validity xy" />
        <Bit Id="DPST-242-600_F-11" Cleared="invalid" Set="valid" Name="Validity brightness" />
    */
    uint16_t x = data[0] << 8 | data[1];
    uint16_t y = data[2] << 8 | data[3];
    uint8_t b = data[4];
    bool xyIgnore = _isGroup ? ParamDGWG_xyIgnore : ParamDGW_xyIgnore;
    if(xyIgnore)
    {
        pushWord(x, currentColor);
        pushWord(y, currentColor + 2);
        
        logDebugP("Got Color: %.4f %.4f", x / 65535.0, y / 65535.0);
    } else {
        ColorHelper::xyyToRGB(x, y, b, currentColor[0], currentColor[1], currentColor[2]);

        uint32_t value = currentColor[0] << 16 | currentColor[1] << 8 | currentColor[2];
        logDebugP("Got Color: %X", value);
    }

    sendColor();

    // use 7.600 to send 2 byte unsigned int
    sendKoStateOnChange(DGW_Kocolor_rgb_state, data, Dpt(7, 600));
}

void DaliChannel::sendKoStateOnChange(uint16_t koNr, const KNXValue &value, const Dpt &type)
{
    GroupObject &ko = knx.getGroupObject(calcKoNumber(koNr));
    if(ko.valueNoSendCompare(value, type))
        ko.objectWritten();
}

void DaliChannel::setTemperature(uint16_t value)
{
    logDebugP("Set Kelvin: %i K", value);
    uint16_t mirek = 1000000.0 / value;
    //TODO check the colorType and then set RGB or TW or do nothing if it is no color Device
    daliMaster.sendSpecialCommand(Dali::SpecialCommand::SET_DTR, mirek & 0xFF);
    daliMaster.sendSpecialCommand(Dali::SpecialCommand::SET_DTR1, (mirek >> 8) & 0xFF);
    daliMaster.sendExtendedCommand(_channelIndex, 0x08, Dali::ExtendedCommandDT8::SET_TEMP_COLOUR_TEMPERATURE, _isGroup);
    daliMaster.sendExtendedCommand(_channelIndex, 0x08, Dali::ExtendedCommandDT8::ACTIVATE, _isGroup);
    sendKoStateOnChange(DGW_Kocolor_rgb_state, value, Dpt(7, 600));
}

void DaliChannel::setBrightness(uint8_t value)
{
    logDebugP("Set Brightness: %i %%", value);
    if (isRgbw())
        rgbwCancel(); // HCL changes the level: the level target of point 3 is no longer valid
    daliMaster.sendArc(_channelIndex, DaliHelper::percentToArc(value), _isGroup);
    sendKoStateOnChange(DGW_Kodimm_state, value, Dpt(5, 1));
}

bool DaliChannel::isDimmOnLocked() {
    uint8_t dimmLock = _isGroup ? ParamDGWG_dimmLock : ParamDGW_dimmLock;
    return dimmLock == PT_dimmLock_noBoth || dimmLock == PT_dimmLock_noOn;
}

bool DaliChannel::isDimmOffLocked() {
    uint8_t dimmLock = _isGroup ? ParamDGWG_dimmLock : ParamDGW_dimmLock;
    return dimmLock == PT_dimmLock_noBoth || dimmLock == PT_dimmLock_noOff;
}

void DaliChannel::queryActualLevel()
{
    if (_isGroup && _dimmReferenceAddress == NoDimmReference)
    {
        logDebugP("Skip QUERY_ACTUAL_LEVEL: no valid dimm reference address set for group");
        return;
    }

    _queryId = daliMaster.sendCommand(_isGroup ? _dimmReferenceAddress : _channelIndex, Dali::Command::QUERY_ACTUAL_LEVEL, false, true);
}

void DaliChannel::sendColor(bool activate)
{
    // TODO only send every 100ms?
    uint8_t r, g, b;
    uint8_t colorType = _isGroup ? ParamDGWG_colorType : ParamDGW_colorType;
    if (colorType == PT_colorType_RGBW)
    {
        // RGBW is always transmitted as RGB level + white level, "colorSpace" does not apply
        if (rgbwSliderMode() && !RgbwHelper::isOff(_mix))
            sendColorAsRGBW(_mix[0], _mix[1], _mix[2], _mix[3], activate); // mix only, the brightness goes as level
        else
            sendColorAsRGBW(currentColor[0], currentColor[1], currentColor[2], currentColor[3], activate);
        return;
    }

    if (colorType == PT_colorType_HSV)
    {
        ColorHelper::hsvToRGB(currentColor[0], currentColor[1], currentColor[2], r, g, b);
    }
    else
    {
        r = currentColor[0];
        g = currentColor[1];
        b = currentColor[2];
    }

    logDebugP("RGB: %i %i %i", r, g, b);
    if (r == 255) r--;
    if (g == 255) g--;
    if (b == 255) b--;
    logDebugP("Send: %i %i %i", r, g, b);

    uint8_t sendType = _isGroup ? ParamDGWG_colorSpace : ParamDGW_colorSpace;
    if (sendType == PT_colorSpace_rgb)
        sendColorAsRGB(r, g, b);
    else
        sendColorAsXY(r, g, b);
}

void DaliChannel::sendColorAsRGB(uint8_t r, uint8_t g, uint8_t b)
{
    daliMaster.sendSpecialCommand(Dali::SpecialCommand::SET_DTR, r);
    daliMaster.sendSpecialCommand(Dali::SpecialCommand::SET_DTR1, g);
    daliMaster.sendSpecialCommand(Dali::SpecialCommand::SET_DTR2, b);
    daliMaster.sendExtendedCommand(_channelIndex, 0x08, Dali::ExtendedCommandDT8::SET_TEMP_RGB_LEVEL, _isGroup);
    daliMaster.sendExtendedCommand(_channelIndex, 0x08, Dali::ExtendedCommandDT8::ACTIVATE, _isGroup);
}

void DaliChannel::sendColorAsRGBW(uint8_t r, uint8_t g, uint8_t b, uint8_t w, bool activate)
{
    r = RgbwHelper::daliLevel(r);
    g = RgbwHelper::daliLevel(g);
    b = RgbwHelper::daliLevel(b);
    w = RgbwHelper::daliLevel(w);
    logDebugP("Send RGBW: %i %i %i %i%s", r, g, b, w, activate ? "" : " (temporary, activated by the following DAPC)");
    // a colour fade starts now (ACTIVATE) or with the DAPC that follows (temporary colour)
    _colourSentAt = millis() | 1; // never 0 (= none)

    // 1. RGB level (DTR0 = R, DTR1 = G, DTR2 = B)
    daliMaster.sendSpecialCommand(Dali::SpecialCommand::SET_DTR, r);
    daliMaster.sendSpecialCommand(Dali::SpecialCommand::SET_DTR1, g);
    daliMaster.sendSpecialCommand(Dali::SpecialCommand::SET_DTR2, b);
    daliMaster.sendExtendedCommand(_channelIndex, 0x08, Dali::ExtendedCommandDT8::SET_TEMP_RGB_LEVEL, _isGroup);

    // 2. WAF level (DTR0 = W, DTR1 = Amber, DTR2 = Freecolour); 255 = MASK = leave unchanged
    daliMaster.sendSpecialCommand(Dali::SpecialCommand::SET_DTR, w);
    daliMaster.sendSpecialCommand(Dali::SpecialCommand::SET_DTR1, 255);
    daliMaster.sendSpecialCommand(Dali::SpecialCommand::SET_DTR2, 255);
    daliMaster.sendExtendedCommand(_channelIndex, 0x08, Dali::ExtendedCommandDT8::SET_TEMP_WAF_LEVEL, _isGroup);

    // 3. Apply both temporary values at once. Without ACTIVATE the following level command (DAPC) activates them.
    if (activate)
        daliMaster.sendExtendedCommand(_channelIndex, 0x08, Dali::ExtendedCommandDT8::ACTIVATE, _isGroup);
}

void DaliChannel::sendColorAsXY(uint8_t r, uint8_t g, uint8_t b)
{
    uint16_t x;
    uint16_t y;

    bool xyIgnore = _isGroup ? ParamDGWG_xyIgnore : ParamDGW_xyIgnore;
    if(xyIgnore)
    {
        popWord(x, currentColor);
        popWord(y, currentColor + 2);
    } else {
        ColorHelper::rgbToXY(r, g, b, x, y);
    }

    daliMaster.sendSpecialCommand(Dali::SpecialCommand::SET_DTR, x & 0xFF);
    daliMaster.sendSpecialCommand(Dali::SpecialCommand::SET_DTR1, (x >> 8) & 0xFF);
    daliMaster.sendExtendedCommand(_channelIndex, 0x08, Dali::ExtendedCommandDT8::SET_COORDINATE_X, _isGroup);

    daliMaster.sendSpecialCommand(Dali::SpecialCommand::SET_DTR, y & 0xFF);
    daliMaster.sendSpecialCommand(Dali::SpecialCommand::SET_DTR1, (y >> 8) & 0xFF);
    daliMaster.sendExtendedCommand(_channelIndex, 0x08, Dali::ExtendedCommandDT8::SET_COORDINATE_Y, _isGroup);

    daliMaster.sendExtendedCommand(_channelIndex, 0x08, Dali::ExtendedCommandDT8::ACTIVATE, _isGroup);
}

void DaliChannel::setSwitchState(bool value, bool isSwitchCommand)
{
    if (isSwitchCommand)
    {
        uint8_t toSet = 0;
        if (value)
        {
            toSet = DaliHelper::percentToArc(isNight ? _onNight : _onDay);
        }
        else
        {
            toSet = 0;
        }

        setDimmState(toSet);
        currentStep = toSet;
    }

    //logDebugP("AutoConfSwitch %i %i %i", value, ParamDGW_hcl_auto_off, _hclIsAutoMode);
    if(!value && (_isGroup ? ParamDGWG_hcl_auto_off : ParamDGW_hcl_auto_off))
        _hclIsAutoMode = true;

    // logDebugP("AutoConfSwitch %i %i %i", value, ParamDGW_hcl_auto_off, _hclIsAutoMode);

    // bool currentState = knx.getGroupObject(calcKoNumber(_isGroup ? DGWG_Koswitch_state : DGW_Koswitch_state)).value(DPT_Switch);
    // if (value == currentState)
    //     return;
    GroupObject &stateKo = knx.getGroupObject(calcKoNumber(_isGroup ? DGWG_Koswitch_state : DGW_Koswitch_state));
    if (isRgbw() && !_isStaircase)
    {
        // RGBW: send the switch status only when it changes (otherwise "on" went out with every absolute dimming
        // command and "off" with every repeated off while dimming down)
        if (stateKo.valueNoSendCompare(value, DPT_Switch))
            stateKo.objectWritten();
    }
    else
        stateKo.value(value, DPT_Switch);
}

void DaliChannel::setDimmState(uint8_t value, bool isDimmCommand, bool isLastCommand)
{
    if (_dimmDirection == DimmDirection::None && value > 0)
    {
        if (isNight)
            _lastNightValue = value;
        else
            _lastDayValue = value;
    }

    if (isDimmCommand)
    {
        setSwitchState(value > 0, false);
        currentStep = value;
    }

    _lastLevelArc = value;
    rgbwSliderStatus(value);

    float perc = DaliHelper::arcToPercentFloat(value);

    GroupObject& ko = knx.getGroupObject(calcKoNumber(DGW_Kodimm_state));
    if(ko.valueNoSendCompare(perc, Dpt(5, 1)))
    {
        logDebugP("SetDimmState %.1f/%i", perc, value);
        ko.objectWritten();
    }
}

void DaliChannel::updateCurrentDimmValue()
{
    if (currentDimmType == DimmType::Color)
        sendColor();

    switch (currentDimmType)
    {
        case DimmType::Brigthness:
        {
            logDebugP("current dimm val %i", *currentDimmValue);
            setDimmState(*currentDimmValue, true, false);
            break;
        }

        case DimmType::Color:
        {
            if ((_isGroup ? ParamDGWG_colorType : ParamDGW_colorType) == PT_colorType_RGBW)
            {
                sendRgbwState();
                break;
            }

            uint32_t value = currentColor[2];
            value |= currentColor[1] << 8;
            value |= currentColor[0] << 16;
            GroupObject& ko = knx.getGroupObject(calcKoNumber(DGW_Kocolor_rgb_state));
            if(ko.valueNoSendCompare(value, Dpt(232, 600)))
                ko.objectWritten();
            
            ko = knx.getGroupObject(calcKoNumber(DGW_Kocolor_red_state));
            if(ko.valueNoSendCompare(currentColor[0], Dpt(5, 1)))
                ko.objectWritten();
                
            ko = knx.getGroupObject(calcKoNumber(DGW_Kocolor_green_state));
            if(ko.valueNoSendCompare(currentColor[1], Dpt(5, 1)))
                ko.objectWritten();
                
            ko = knx.getGroupObject(calcKoNumber(DGW_Kocolor_blue_state));
            if(ko.valueNoSendCompare(currentColor[2], Dpt(5, 1)))
                ko.objectWritten();
            break;
        }
    }
}

void DaliChannel::setOnValue(uint8_t value)
{
    _onDay = value;
}

void DaliChannel::setGroups(uint16_t groupBits, DaliChannel* groups)
{
    _groups = groupBits;

    // Set Reference Address to get current value from evg
    // when relative dimming a group
    if(!_isGroup)
    {
        for(int i = 0; i < GroupCount; i++)
        {
            if((groupBits >> i) & 1)
            {
                groups[i].setDimmRef(_channelIndex);
            }
        }
    }
}

void DaliChannel::setDimmRef(uint8_t ref)
{
    if(_isGroup && _dimmReferenceAddress == NoDimmReference)
    {
        logDebugP("Set Dimmref for %u to %u", _channelIndex, ref);
        _dimmReferenceAddress = ref;
    }
}

void DaliChannel::setGroupState(uint16_t group, bool state)
{
    if (group == AllGroups || _groups & 1 << group)
    {
        setSwitchState(state, false);
        // RGBW: the on/off flag follows what the driver reports / a group or broadcast command did. A pending window
        // and the level target of this channel are no longer valid.
        if (isRgbw() && !_isStaircase)
        {
            rgbwCancel();
            currentState = state;
        }
    }
}

void DaliChannel::setGroupState(uint16_t group, uint8_t value)
{
    if (group == AllGroups || _groups & (1 << group))
    {
        setDimmState(value, true, true);
        // RGBW: also at start (level read from the driver): a lamp that is lit is "on" for us, otherwise "brighter"
        // would jump to the minimum. A pending window and the level target are no longer valid.
        if (isRgbw() && !_isStaircase)
        {
            rgbwCancel();
            currentState = value > 0;
        }
    }
}

void DaliChannel::setMinArc(uint8_t min)
{
    _min = min;
}

void DaliChannel::setMaxArc(uint8_t max)
{
    _max = max;
}

void DaliChannel::setHcl(uint8_t curve, uint16_t value)
{
    //TODO add ComObject for selecting curve
    if(_hclCurve == HclDisabled) return;
    logDebugP("Temp curve %i | state %i | isAlsoOn %i", _hclCurve, currentState, _hclIsAlsoOn);
    if(!_hclIsAutoMode)
    {
        logDebugP("Ignored because mode is manu");
        return;
    }

    if(curve == _hclCurve)
        _hclCurrentTemp = value;

    if(curve == _hclCurve && currentState)
    {
        if(_hclIsAlsoOn)
        {
            logDebugP("Setting HCL");
            setTemperature(value);
        } else {
            logDebugP("Ignored. Only apply on turning on");
        }
    }
}

void DaliChannel::setHcl(uint8_t curve, uint8_t value)
{
    if(_hclCurve == HclDisabled) return;
    logDebugP("Bri curve %i | state %i | isAlsoOn %i", _hclCurve, currentState, _hclIsAlsoOn);
    if(!_hclIsAutoMode)
    {
        logDebugP("Ignored because mode is manu");
        return;
    }

    if(curve == _hclCurve)
        _hclCurrentBri = value;

    if(curve == _hclCurve && currentState)
    {
        _hclCurrentBri = value;
        if(_hclIsAlsoOn)
        {
            logDebugP("Setting HCL");
            setBrightness(value);
        } else {
            logDebugP("Ignored. Only apply on turning on");
        }
    }
}

uint8_t DaliChannel::getMin()
{
    return DaliHelper::arcToPercent(_min);
}

uint8_t DaliChannel::getMax()
{
    return DaliHelper::arcToPercent(_max);
}

uint16_t DaliChannel::getGroups()
{
    return _groups;
}