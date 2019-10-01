from conans import ConanFile, tools


class CiscodiscoveryConan(ConanFile):
    name = "HostManager"
    version = "0.1"
    settings = None
    description = "Find new hosts around topology"
    url = "None"
    license = "None"
    author = "None"
    topics = None

    def package(self):
        self.copy("*")

    def package_info(self):
        self.cpp_info.libs = tools.collect_libs(self)
