#pragma once

#include "settings.h"

class SettingsStore {
public:
    Settings load() const;
    void save(const Settings &settings) const;
};
