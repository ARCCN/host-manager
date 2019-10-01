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

#include "RestListener.hpp"

#include <boost/lexical_cast.hpp>

namespace runos {

struct HostCollection : rest::resource {
    HostManager* app;

    explicit HostCollection(HostManager* _app) : app(_app) {}

    rest::ptree Get() const override {
        rest::ptree root;

        for (auto h_pair: app->hosts()) {
            rest::ptree host;
            auto h = h_pair.second;

            host.put("ID", boost::lexical_cast<std::string>(h->id()));
            host.put("mac", h->mac());
            host.put("switch_dpid", boost::lexical_cast<std::string>(h->switchID()));
            host.put("switch_port", (int)h->switchPort());

            root.add_child(h->mac().c_str(), host);
        }
        return root;
    }
};

class HostManagerRest : public Application {
    SIMPLE_APPLICATION(HostManagerRest, "host-manager-rest")
public:
    void init(Loader *loader, const Config&) override
    {
        using rest::path_spec;
        using rest::path_match;

        auto app = HostManager::get(loader);
        auto rest_ = RestListener::get(loader);

        rest_->mount(path_spec("/hosts/"), [=](const path_match&) {
            return HostCollection {app};
        });
    }

};

REGISTER_APPLICATION(HostManagerRest, {"rest-listener", "host-manager", ""})

} // namespace runos
