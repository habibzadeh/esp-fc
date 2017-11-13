#ifndef _ESPFC_INPUT_H_
#define _ESPFC_INPUT_H_

#include "Model.h"
#include "Math.h"
#include "Hardware.h"
#include "InputPPM.h"
#include "InputSBUS.h"

namespace Espfc {

enum FailsafeChannelMode {
  FAILSAFE_MODE_AUTO,
  FAILSAFE_MODE_HOLD,
  FAILSAFE_MODE_SET
};

class Input
{
  public:
    Input(Model& model): _model(model) {}
    int begin()
    {
      _device = Hardware::getInputDevice(_model);
      setFailsafe();
      return 1;
    }

    void setFailsafe()
    {
      for(size_t i = 0; i < INPUT_CHANNELS; ++i)
      {
        if(_model.config.failsafeMode[i] == FAILSAFE_MODE_HOLD) continue;
        _model.state.inputUs[i] = _model.config.failsafeValue[i];
        _model.state.input[i] = Math::map3((float)_get(i, 0), _model.config.inputMin[i], _model.config.inputNeutral[i], _model.config.inputMax[i], -1.f, 0.f, 1.f);
      }
    }

    int update()
    {
      _model.state.stats.start(COUNTER_INPUT);
      int ret = doUpdate();
      _model.state.stats.end(COUNTER_INPUT);
      return ret;
    }

    int doUpdate()
    {
      if(!_device) return 0;

      static float step = 0;
      static float inputDt = 0.02f;
      static uint32_t prevTm = 0;

      InputStatus status = _device->update();

      if(status == INPUT_FAILED)
      {
        setFailsafe();
        _model.state.buzzer.play(BEEPER_RX_LOST);
        _model.state.inputLinkValid = false;
        return 0;
      }

      if(status == INPUT_RECEIVED)
      {
        switch(_model.config.inputInterpolation)
        {
          case INPUT_INTERPOLATION_AUTO:
            {
              uint32_t now = micros();
              inputDt = Math::bound(now - prevTm, (uint32_t)4000, (uint32_t)30000) * 0.000001f;
              prevTm = now;
            }
            break;
          case INPUT_INTERPOLATION_MANUAL:
            inputDt = _model.config.inputInterpolationInterval * 0.001f;
            break;
          default:
            inputDt = 0.02f;
            break;
        }
        _model.state.inputLinkValid = true;
        _read();
        step = 0.f;
      }

      if(_model.config.inputInterpolation != INPUT_INTERPOLATION_OFF)
      {
        float interpolationStep = _model.state.loopTimer.delta / inputDt;
        if(step < 1.f) step += interpolationStep;
        for(size_t i = 0; i < INPUT_CHANNELS; ++i)
        {
          float curr = Math::map3((float)_get(i, 0), _model.config.inputMin[i], _model.config.inputNeutral[i], _model.config.inputMax[i], -1.f, 0.f, 1.f);
          float prev = Math::map3((float)_get(i, 1), _model.config.inputMin[i], _model.config.inputNeutral[i], _model.config.inputMax[i], -1.f, 0.f, 1.f);
          float val =  i < 3 ? Math::bound(_interpolate(prev, curr, step), -1.f, 1.f) : curr;
          //_model.state.input[i] = _model.state.input[i] * (1.f - _model.state.inputFilterAlpha) + val * _model.state.inputFilterAlpha;
          _model.state.input[i] = val;
          _model.state.inputUs[i] = (uint16_t)lrintf(Math::map3(_model.state.input[i], -1.f, 0.f, 1.f, _model.config.inputMin[i], _model.config.inputNeutral[i], _model.config.inputMax[i]));
        }
      }
      else if(status == INPUT_RECEIVED)
      {
        for(size_t i = 0; i < INPUT_CHANNELS; ++i)
        {
          float val = Math::map3((float)_get(i, 0), _model.config.inputMin[i], _model.config.inputNeutral[i], _model.config.inputMax[i], -1.f, 0.f, 1.f);
          _model.state.input[i] = val;
          _model.state.inputUs[i] = (uint16_t)lrintf(Math::map3(_model.state.input[i], -1.f, 0.f, 1.f, _model.config.inputMin[i], _model.config.inputNeutral[i], _model.config.inputMax[i]));
        }
      }

      return 1;
    }

  private:
    float _interpolate(float left, float right, float step)
    {
      return (left * (1.f - step) + right * step);
    }

    void _read()
    {
      _shift();
      for(size_t c = 0; c < INPUT_CHANNELS; ++c)
      {
        int pulse = _device->get(_model.config.inputMap[c]);
        _set(c, pulse);
      }
    }

    void _shift()
    {
      for(size_t b = INPUT_BUFF_SIZE - 1; b > 0; b--)
      {
        for(size_t c = 0; c < INPUT_CHANNELS; c++)
        {
          _buff[b][c] = _buff[b - 1][c];
        }
      }
    }

    uint32_t _get(size_t c, size_t b)
    {
      //return _buff[b][i];
      return (_buff[b][c] + _buff[b + 1][c]) / 2; // avg last two samples
    }

    void _set(size_t c, int16_t v)
    {
      if(c < 3)
      {
        v = (int16_t)Math::deadband((int)v - _model.config.inputMidRc, (int)_model.config.inputDeadband) + _model.config.inputMidRc;
      }
      _buff[0][c] = v;
    }
    static const size_t INPUT_BUFF_SIZE = 3;
    static const size_t INPUT_BUFF_SIZE_HALF = INPUT_BUFF_SIZE / 2;

    Model& _model;
    int16_t _buff[INPUT_BUFF_SIZE][INPUT_CHANNELS];
    InputDevice * _device;
};

}

#endif
