#ifndef _ESPFC_SERIAL_DEVICE_H_
#define _ESPFC_SERIAL_DEVICE_H_

#include <Arduino.h>
#include "EspSoftSerial.h"

#define SERIAL_RXD_INV (1  <<  UCRXI) // bit 19 - invert rx
#define SERIAL_TXD_INV (1  <<  UCTXI) // bit 22 - invert tx

namespace Espfc {

struct SerialDeviceConfig
{
  int32_t baud;
  int8_t rx_pin;
  int8_t tx_pin;
  bool inverted;
  int8_t stop_bits;
  int8_t parity;
};

class SerialDevice: public Stream
{
  public:
    virtual void begin(const SerialDeviceConfig& conf) = 0;
    virtual int available() = 0;
    virtual int read() = 0;
    virtual int peek() = 0;
    virtual void flush() = 0;
    virtual size_t write(uint8_t c) = 0;
    virtual size_t availableForWrite() = 0;
    using Print::write;
};

template<typename T>
class SerialDeviceAdapter: public SerialDevice
{
  public:
   SerialDeviceAdapter(T * dev): _dev(dev) {}
   virtual void begin(const SerialDeviceConfig& conf);
   virtual int available() { return _dev->available(); }
   virtual int read() { return _dev->read(); }
   virtual int peek() { return _dev->peek(); }
   virtual void flush() { _dev->flush(); }
   virtual size_t write(uint8_t c) { return _dev->write(c); }
   virtual size_t availableForWrite() { return _dev->availableForWrite(); }
  private:
    T * _dev;
};

template<>
void SerialDeviceAdapter<HardwareSerial>::begin(const SerialDeviceConfig& conf)
{
  int sc = SERIAL_8N1;
  if(conf.inverted)
  {
    sc |= SERIAL_RXD_INV | SERIAL_TXD_INV;
  }
  _dev->begin(conf.baud, (SerialConfig)sc);
}

template<>
void SerialDeviceAdapter<EspSoftSerial>::begin(const SerialDeviceConfig& conf)
{
  EspSoftSerialConfig ec;
  ec.baud = conf.baud;
  ec.rx_pin = conf.rx_pin;
  ec.inverted = conf.inverted;
  _dev->begin(ec);
}

}

#endif // _ESPFC_SERIAL_DEVICE_H_
