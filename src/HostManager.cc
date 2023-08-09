/*
 * Copyright 2015 Applied Research Center for Computer Networks
 *
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

#include "HostManager.hpp"

#include "Controller.hpp"
#include "PacketParser.hpp"
#include "oxm/openflow_basic.hh"

#include <boost/lexical_cast.hpp>
#include <fluid/util/ipaddr.hh>

namespace runos {

REGISTER_APPLICATION(HostManager, {"switch-manager", ""})

struct HostImpl {
    uint64_t id;
    std::string mac;
    ipv4addr ip;
    uint64_t switchID;
    uint32_t switchPort;
};

struct HostManagerImpl {
    // mac address -> Host
    std::unordered_map<std::string, Host*> hosts;
};

Host::Host(std::string mac, ipv4addr ip)
{
    m = new HostImpl;
    m->mac = mac;
    m->ip = ip;
    m->id = rand()%1000 + 1000;
}

Host::~Host()
{ delete m; }

uint64_t Host::id() const
{ return m->id; }

std::string Host::mac() const
{ return m->mac; }

std::string Host::ip() const
{ return boost::lexical_cast<std::string>(m->ip); }

uint64_t Host::switchID() const
{ return m->switchID; }

uint32_t Host::switchPort() const
{ return m->switchPort; }

json11::Json Host::to_json() const
{
    return json11::Json::object {
        {"ID", boost::lexical_cast<std::string>(id())},
        {"mac", mac()},
        {"switch_id", boost::lexical_cast<std::string>(switchID())},
        {"switch_port", (int)switchPort()}
    };
}

void Host::switchID(uint64_t id)
{ m->switchID = id; }

void Host::switchPort(uint32_t port)
{ m->switchPort = port; }

void Host::ip(std::string ip)
{ m->ip = convert(ip).first; }

void Host::ip(ipv4addr ip)
{ m->ip = ipv4addr(ip); }

HostManager::HostManager()
{
    m = new HostManagerImpl;
}

HostManager::~HostManager()
{ delete m; }

void HostManager::init(Loader *loader, const Config &config)
{
    m_switch_manager = SwitchManager::get(loader);

    const auto ofb_in_port = oxm::in_port();
    const auto ofb_eth_type = oxm::eth_type();
    const auto ofb_eth_src = oxm::eth_src();
    const auto ofb_arp_spa = oxm::arp_spa();
    const auto ofb_ipv4_src = oxm::ipv4_src();

    handler = Controller::get(loader)->register_handler(
            [=](of13::PacketIn& pi, OFConnectionPtr connection) {
                PacketParser pp(pi);
                Packet& pkt(pp);

                ethaddr eth_src = pkt.load(ofb_eth_src);
                std::string host_mac = boost::lexical_cast<std::string>(eth_src);

                ipv4addr host_ip(convert("0.0.0.0").first);
                if (pkt.test(ofb_eth_type == 0x0800)) {
                    host_ip = ipv4addr(pkt.load(ofb_ipv4_src));
                } else if (pkt.test(ofb_eth_type == 0x0806)) {
                    host_ip = ipv4addr(pkt.load(ofb_arp_spa));
                }

                if (isSwitch(host_mac))
                    return false;

                uint32_t in_port = pkt.load(ofb_in_port);
                if (in_port > of13::OFPP_MAX)
                    return false;

                if (not findMac(host_mac)) {
                    auto sw =
                        m_switch_manager->switch_(connection->dpid());
                    addHost(sw, host_ip, host_mac, in_port);

                    LOG(INFO) << "Host discovered. MAC: " << host_mac
                              << ", IP: " << host_ip
                              << ", Switch ID: " << sw->dpid() << ", port: " << in_port;
                } else {
                    Host* h = getHost(host_mac);
                    if (host_ip != convert("0.0.0.0").first) {
                        h->ip(host_ip);
                    }
                }

                return false;
        }, -40
    );

    QObject::connect(m_switch_manager, &SwitchManager::switchUp,
                     this, &HostManager::onSwitchDiscovered);
    QObject::connect(m_switch_manager, &SwitchManager::switchDown,
                     this, &HostManager::onSwitchDown);

}

void HostManager::onSwitchDiscovered(SwitchPtr dp)
{
    QObject::connect(dp.get(), &Switch::portAdded, this, &HostManager::newPort);
    for (const auto& port : dp->ports()) {
        newPort(port);
    }
}

void HostManager::onSwitchDown(SwitchPtr dp)
{
    delHostForSwitch(dp);
    for (auto port : dp->ports()) {
        auto pos = std::find(switch_macs.begin(), switch_macs.end(), boost::lexical_cast<std::string>(port->hw_addr()));
        if (pos != switch_macs.end())
            switch_macs.erase(pos);
    }
}

void HostManager::addHost(SwitchPtr sw, ipv4addr ip, std::string mac, uint32_t port)
{
    std::lock_guard<std::mutex> lk(mutex);

    Host* dev = createHost(mac, ip);
    attachHost(mac, sw->dpid(), port);
    emit hostDiscovered(dev);
}

Host* HostManager::createHost(std::string mac, ipv4addr ip)
{
    Host* dev = new Host(mac, ip);
    m->hosts[mac] = dev;
    return dev;
}

bool HostManager::findMac(std::string mac)
{
    if (m->hosts.count(mac) > 0)
        return true;
    return false;
}

bool HostManager::isSwitch(std::string mac)
{
    for (auto sw_mac : switch_macs) {
        if (sw_mac == mac)
            return true;
    }
    return false;
}

void HostManager::attachHost(std::string mac, uint64_t id, uint32_t port)
{
    m->hosts[mac]->switchID(id);
    m->hosts[mac]->switchPort(port);
}

void HostManager::delHostForSwitch(SwitchPtr dp)
{
    auto itr = m->hosts.begin();
    while (itr != m->hosts.end()) {
        if (itr->second->switchID() == dp->dpid()) {
            m->hosts.erase(itr++);
        }
        else
            ++itr;
    }
}

Host* HostManager::getHost(std::string mac)
{
    if (m->hosts.count(mac) > 0)
        return m->hosts[mac];
    else
        return nullptr;
}

Host* HostManager::getHost(ipv4addr ip)
{
    auto string_ip = boost::lexical_cast<std::string>(ip);
    for (auto it : m->hosts) {
        if (it.second->ip() == string_ip)
            return it.second;
    }
    return nullptr;
}

void HostManager::newPort(PortPtr port)
{
    switch_macs.push_back(boost::lexical_cast<std::string>(port->hw_addr()));
}

std::unordered_map<std::string, Host*> HostManager::hosts()
{
    return m->hosts;
}


} // namespace runos
