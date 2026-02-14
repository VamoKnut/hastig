#pragma once

#include <stdint.h>
#include <atomic>

#include "RestartReason.h"

/**
 * @brief Hibernate request issued by Orchestrator and executed from Arduino loop().
 */
struct HibernateRequest {
  bool             requested         = false;
  RestartReasonCode reasonCode       = RestartReasonCode::LowPowerWakeup;
  uint32_t         expectedDurationS = 0;
};

/**
 * @brief Global hibernate request manager.
 */
class HibernateManager {
public:
  void request(RestartReasonCode code, uint32_t durationS);
  bool consume(HibernateRequest& out);

private:
  std::atomic<bool> _req{false};
  HibernateRequest  _last;
};

extern HibernateManager gHibernate;
