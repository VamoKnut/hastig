#include "HibernateManager.h"

HibernateManager gHibernate;

void HibernateManager::request(RestartReasonCode code, uint32_t durationS)
{
  _last.requested         = true;
  _last.reasonCode        = code;
  _last.expectedDurationS = durationS;
  _req.store(true);
}

bool HibernateManager::consume(HibernateRequest& out)
{
  if (!_req.exchange(false)) {
    return false;
  }
  out = _last;
  return true;
}
