#include <signal.h>
#include <pwd.h>
#include <event2/event.h>
#include <string.h>

#include <unordered_map>
#include <phosg/JSON.hh>
#include <phosg/Network.hh>
#include <phosg/Strings.hh>
#include <phosg/Filesystem.hh>
#include <set>

#include "NetworkAddresses.hh"
#include "SendCommands.hh"
#include "DNSServer.hh"
#include "ProxyServer.hh"
#include "ServerState.hh"
#include "Server.hh"
#include "FileContentsCache.hh"
#include "Text.hh"
#include "ServerShell.hh"
#include "ProxyShell.hh"

using namespace std;



FileContentsCache file_cache;



static const unordered_map<string, PortConfiguration> default_port_to_behavior({
  {"gc-jp10",  {9000,  GameVersion::GC,    ServerBehavior::LoginServer}},
  {"gc-jp11",  {9001,  GameVersion::GC,    ServerBehavior::LoginServer}},
  {"gc-jp3",   {9003,  GameVersion::GC,    ServerBehavior::LoginServer}},
  {"gc-us10",  {9100,  GameVersion::PC,    ServerBehavior::SplitReconnect}},
  {"gc-us3",   {9103,  GameVersion::GC,    ServerBehavior::LoginServer}},
  {"gc-eu10",  {9200,  GameVersion::GC,    ServerBehavior::LoginServer}},
  {"gc-eu11",  {9201,  GameVersion::GC,    ServerBehavior::LoginServer}},
  {"gc-eu3",   {9203,  GameVersion::GC,    ServerBehavior::LoginServer}},
  {"pc-login", {9300,  GameVersion::PC,    ServerBehavior::LoginServer}},
  //{"pc-patch", {10000, GameVersion::Patch, ServerBehavior::PatchServer}},
  {"bb-patch", {11000, GameVersion::Patch, ServerBehavior::PatchServer}},
  {"bb-data",  {12000, GameVersion::BB,    ServerBehavior::DataServerBB}},

  // these aren't hardcoded in any games; user can override them
  {"bb-data1", {12004, GameVersion::BB,    ServerBehavior::DataServerBB}},
  {"bb-data2", {12005, GameVersion::BB,    ServerBehavior::DataServerBB}},
  {"bb-login", {12008, GameVersion::BB,    ServerBehavior::LoginServer}},
  {"pc-lobby", {9420,  GameVersion::PC,    ServerBehavior::LobbyServer}},
  {"gc-lobby", {9421,  GameVersion::GC,    ServerBehavior::LobbyServer}},
  {"bb-lobby", {9422,  GameVersion::BB,    ServerBehavior::LobbyServer}},
});



template <typename T>
vector<T> parse_int_vector(shared_ptr<const JSONObject> o) {
  vector<T> ret;
  for (const auto& x : o->as_list()) {
    ret.emplace_back(x->as_int());
  }
  return ret;
}



void populate_state_from_config(shared_ptr<ServerState> s,
    shared_ptr<JSONObject> config_json) {
  const auto& d = config_json->as_dict();

  s->name = decode_sjis(d.at("ServerName")->as_string());

  try {
    s->username = d.at("User")->as_string();
    if (s->username == "$SUDO_USER") {
      const char* user_from_env = getenv("SUDO_USER");
      if (!user_from_env) {
        throw runtime_error("configuration specifies $SUDO_USER, but variable is not defined");
      }
      s->username = user_from_env;
    }
  } catch (const out_of_range&) { }

  // TODO: make this configurable
  s->port_configuration = default_port_to_behavior;

  auto enemy_categories = parse_int_vector<uint32_t>(d.at("CommonItemDropRates-Enemy"));
  auto box_categories = parse_int_vector<uint32_t>(d.at("CommonItemDropRates-Box"));
  vector<vector<uint8_t>> unit_types;
  for (const auto& item : d.at("CommonUnitTypes")->as_list()) {
    unit_types.emplace_back(parse_int_vector<uint8_t>(item));
  }
  s->common_item_creator.reset(new CommonItemCreator(enemy_categories,
      box_categories, unit_types));

  shared_ptr<vector<MenuItem>> information_menu(new vector<MenuItem>());
  shared_ptr<vector<u16string>> information_contents(new vector<u16string>());

  information_menu->emplace_back(INFORMATION_MENU_GO_BACK, u"Go back",
      u"Return to the\nmain menu", 0);

  uint32_t item_id = 0;
  for (const auto& item : d.at("InformationMenuContents")->as_list()) {
    auto& v = item->as_list();
    information_menu->emplace_back(item_id, decode_sjis(v.at(0)->as_string()),
        decode_sjis(v.at(1)->as_string()), MenuItemFlag::RequiresMessageBoxes);
    information_contents->emplace_back(decode_sjis(v.at(2)->as_string()));
    item_id++;
  }
  s->information_menu = information_menu;
  s->information_contents = information_contents;

  try {
    s->welcome_message = decode_sjis(d.at("WelcomeMessage")->as_string());
  } catch (const out_of_range&) { }

  auto local_address_str = d.at("LocalAddress")->as_string();
  try {
    s->local_address = s->all_addresses.at(local_address_str);
    string addr_str = string_for_address(s->local_address);
    log(INFO, "added local address: %s (%s)", addr_str.c_str(),
        local_address_str.c_str());
  } catch (const out_of_range&) {
    s->local_address = address_for_string(local_address_str.c_str());
    log(INFO, "added local address: %s", local_address_str.c_str());
  }
  s->all_addresses.emplace("<local>", s->local_address);

  auto external_address_str = d.at("ExternalAddress")->as_string();
  try {
    s->external_address = s->all_addresses.at(external_address_str);
    string addr_str = string_for_address(s->external_address);
    log(INFO, "added external address: %s (%s)", addr_str.c_str(),
        external_address_str.c_str());
  } catch (const out_of_range&) {
    s->external_address = address_for_string(external_address_str.c_str());
    log(INFO, "added external address: %s", external_address_str.c_str());
  }
  s->all_addresses.emplace("<external>", s->external_address);

  try {
    s->run_dns_server = d.at("RunDNSServer")->as_bool();
  } catch (const out_of_range&) {
    s->run_dns_server = true;
  }

  try {
    s->allow_unregistered_users = d.at("AllowUnregisteredUsers")->as_bool();
  } catch (const out_of_range&) {
    s->allow_unregistered_users = true;
  }

  try {
    bool run_shell = d.at("RunInteractiveShell")->as_bool();
    s->run_shell_behavior = run_shell ?
        ServerState::RunShellBehavior::Always :
        ServerState::RunShellBehavior::Never;
  } catch (const out_of_range&) { }
}



void drop_privileges(const string& username) {
  if ((getuid() != 0) || (getgid() != 0)) {
    throw runtime_error(string_printf(
        "newserv was not started as root; can\'t switch to user %s",
        username.c_str()));
  }

  struct passwd* pw = getpwnam(username.c_str());
  if (!pw) {
    string error = string_for_error(errno);
    throw runtime_error(string_printf("user %s not found (%s)",
        username.c_str(), error.c_str()));
  }

  if (setgid(pw->pw_gid) != 0) {
    string error = string_for_error(errno);
    throw runtime_error(string_printf("can\'t switch to group %d (%s)",
        pw->pw_gid, error.c_str()));
  }
  if (setuid(pw->pw_uid) != 0) {
    string error = string_for_error(errno);
    throw runtime_error(string_printf("can\'t switch to user %d (%s)",
        pw->pw_uid, error.c_str()));
  }
  log(INFO, "switched to user %s (%d:%d)",  username.c_str(), pw->pw_uid,
      pw->pw_gid);
}



int main(int argc, char* argv[]) {
  log(INFO, "fuzziqer software newserv");

  string proxy_hostname;
  int proxy_port = 0;
  for (int x = 1; x < argc; x++) {
    if (!strncmp(argv[x], "--proxy-destination=", 20)) {
      auto netloc = parse_netloc(&argv[x][20], 9100);
      proxy_hostname = netloc.first;
      proxy_port = netloc.second;
    } else {
      throw invalid_argument(string_printf("unknown option: %s", argv[x]));
    }
  }

  signal(SIGPIPE, SIG_IGN);

  shared_ptr<ServerState> state(new ServerState());

  log(INFO, "starting network subsystem");
  shared_ptr<struct event_base> base(event_base_new(), event_base_free);

  log(INFO, "reading network addresses");
  state->all_addresses = get_local_addresses();
  for (const auto& it : state->all_addresses) {
    string addr_str = string_for_address(it.second);
    log(INFO, "found interface: %s = %s", it.first.c_str(), addr_str.c_str());
  }

  log(INFO, "loading configuration");
  auto config_json = JSONObject::load("system/config.json");
  populate_state_from_config(state, config_json);

  shared_ptr<DNSServer> dns_server;
  if (state->run_dns_server) {
    log(INFO, "starting dns server");
    dns_server.reset(new DNSServer(base, state->local_address,
        state->external_address));
    dns_server->listen("", 53);
  }

  shared_ptr<ProxyServer> proxy_server;
  shared_ptr<Server> game_server;
  if (proxy_port) {
    log(INFO, "starting proxy");
    sockaddr_storage proxy_destination_ss = make_sockaddr_storage(
        proxy_hostname, proxy_port).first;
    proxy_server.reset(new ProxyServer(base, proxy_destination_ss, proxy_port));

  } else {
    log(INFO, "starting game server");
    game_server.reset(new Server(base, state));
    for (const auto& it : state->port_configuration) {
      game_server->listen("", it.second.port, it.second.version, it.second.behavior);
    }

    log(INFO, "loading license list");
    state->license_manager.reset(new LicenseManager("system/licenses.nsi"));

    log(INFO, "loading battle parameters");
    state->battle_params.reset(new BattleParamTable("system/blueburst/BattleParamEntry"));

    log(INFO, "loading level table");
    state->level_table.reset(new LevelTable("system/blueburst/PlyLevelTbl.prs", true));

    log(INFO, "collecting quest metadata");
    state->quest_index.reset(new QuestIndex("system/quests"));
  }

  if (!state->username.empty()) {
    log(INFO, "switching to user %s", state->username.c_str());
    drop_privileges(state->username);
  }

  bool should_run_shell = (state->run_shell_behavior == ServerState::RunShellBehavior::Always);
  if (state->run_shell_behavior == ServerState::RunShellBehavior::Default) {
    should_run_shell = isatty(fileno(stdin));
  }

  shared_ptr<Shell> shell;
  if (should_run_shell) {
    log(INFO, "starting interactive shell");
    if (proxy_port) {
      shell.reset(new ProxyShell(base, state, proxy_server));
    } else {
      shell.reset(new ServerShell(base, state));
    }
  }

  log(INFO, "ready");
  event_base_dispatch(base.get());

  log(INFO, "normal shutdown");
  return 0;
}
