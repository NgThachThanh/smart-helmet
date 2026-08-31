#ifndef VAPCO_H
#define VAPCO_H

class SmartHelmetSensor {
public:
  SmartHelmetSensor(int sda, int scl, int led);
  ~SmartHelmetSensor();
  void begin();
  void update();

private:
  struct Impl;
  Impl* impl;
};

#endif
