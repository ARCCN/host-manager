 /*
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/** @file */
#pragma once

#include "Loader.hpp"
#include "Application.hpp"
#include "SwitchManager.hpp"
#include "Controller.hpp"

#include "lib/ipv4addr.hpp"

#include <mutex>
#include <string>
#include <vector>
#include <unordered_map>
#include <time.h>

namespace runos {

namespace of13 = fluid_msg::of13;

/**
 * Host object corresponding end host
 */
class Host {
    struct HostImpl* m;
public:
    uint64_t id() const;
    json11::Json to_json() const;
    std::string mac() const;
    std::string ip() const;
    uint64_t switchID() const;
    uint32_t switchPort()const;
    void switchID(uint64_t id);
    void switchPort(uint32_t port);
    void ip(std::string ip);
    void ip(ipv4addr ip);

    Host(std::string mac, ipv4addr ip);
    ~Host();
};

/**
 * HostManager is looking for new hosts in the network.
 * This application connects switch-manager's signal &SwitchManager::switchDiscovered
 * and remember all switches and their mac addresses.
 *
 * Also application handles all packet_ins and if eth_src field is not belongs to switch
 * it means that new end host appears in the network.
 */
class HostManager: public Application {
    Q_OBJECT
    SIMPLE_APPLICATION(HostManager, "host-manager")
public:
    HostManager();
    ~HostManager();

    void init(Loader* loader, const Config& config) override;

    std::unordered_map<std::string, Host*> hosts();
    Host* getHost(std::string mac);
    Host* getHost(ipv4addr ip);

public slots:
    void onSwitchDiscovered(SwitchPtr dp);
    void onSwitchDown(SwitchPtr dp);
    void newPort(PortPtr port);
signals:
    void hostDiscovered(Host* dev);
private:
    struct HostManagerImpl* m;
    OFMessageHandlerPtr handler;
    std::vector<std::string> switch_macs;
    SwitchManager* m_switch_manager;
    std::mutex mutex;

    void addHost(SwitchPtr sw, ipv4addr ip, std::string mac, uint32_t port);
    Host* createHost(std::string mac, ipv4addr ip);
    bool findMac(std::string mac);
    bool isSwitch(std::string mac);
    void attachHost(std::string mac, uint64_t id, uint32_t port);
    void delHostForSwitch(SwitchPtr dp);
};

} // namespace runos
