#pragma once
static bool camVisionMayRetryResponse(bool sawSse,int result,bool outputLimited,bool cancelled) {
  return !sawSse && !outputLimited && !cancelled && result!=-5 && result!=-6 && result!=-11;
}
