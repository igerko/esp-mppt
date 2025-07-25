// IModem.h
#pragma once
#include <LittleFS.h>

#include "Globals.h"
#include "secrets.h"
#include "TimeService.h"
#include "TinyGsmClient.h"

class ICommunicationService
{
public:
    virtual ~ICommunicationService() = default;

    virtual std::optional<timeval> getTimeFromModem() = 0;

    virtual int getSignalStrengthPercentage() = 0;

    virtual void downloadConfig() = 0;
    virtual void sendMPPTPayload() = 0;

    void setupModem()
    {
        setupModemImpl();
        setupTime();
        isModemOn_ = true;
    }

    void powerOffModem()
    {
        if (isModemOn_)
            powerOffModemImpl();
        isModemOn_ = false;
    }

    [[nodiscard]] bool isModemOn() const { return isModemOn_; }


protected:

    virtual void setupModemImpl() = 0;
    virtual void powerOffModemImpl() = 0;

    void setupTime()
    {
        if (const auto timeVal = getTimeFromModem())
        {
            TimeService::setESPTimeFromModem(*timeVal);
        }
    }

private:
    bool isModemOn_ = false;
};
