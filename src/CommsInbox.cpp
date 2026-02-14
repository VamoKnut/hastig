#include "CommsInbox.h"

CommsInbox::CommsInbox(AggMailT& aggToCommsMail, OrchToCommsMailT& orchToCommsMail)
    : _aggToCommsMail(aggToCommsMail),
      _orchToCommsMail(orchToCommsMail)
{
}

OrchCommandMsg* CommsInbox::tryGetOrch()
{
  return _orchToCommsMail.try_get();
}

void CommsInbox::freeOrch(OrchCommandMsg* msg)
{
  _orchToCommsMail.free(msg);
}

AggregateMsg* CommsInbox::tryGetAggregate()
{
  return _aggToCommsMail.try_get();
}

void CommsInbox::freeAggregate(AggregateMsg* msg)
{
  _aggToCommsMail.free(msg);
}
