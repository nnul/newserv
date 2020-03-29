#include "ServerState.hh"

#include <memory>

#include "SendCommands.hh"
#include "NetworkAddresses.hh"
#include "Text.hh"

using namespace std;



ServerState::ServerState() : run_dns_server(true),
    allow_unregistered_users(false),
    run_shell_behavior(RunShellBehavior::Default), next_lobby_id(1),
    pre_lobby_event(0) {
  this->main_menu.emplace_back(MAIN_MENU_GO_TO_LOBBY, u"Go to lobby",
      u"Join the lobby.", 0);
  this->main_menu.emplace_back(MAIN_MENU_INFORMATION, u"Information",
      u"View server information.", MenuItemFlag::RequiresMessageBoxes);
  this->main_menu.emplace_back(MAIN_MENU_DISCONNECT, u"Disconnect",
      u"Disconnect.", 0);

  for (size_t x = 0; x < 20; x++) {
    auto lobby_name = decode_sjis(string_printf("LOBBY%zu", x + 1));
    shared_ptr<Lobby> l(new Lobby());
    l->flags |= LobbyFlag::Public | LobbyFlag::Default | LobbyFlag::Persistent |
        ((x > 14) ? LobbyFlag::Episode3 : 0);
    l->block = x + 1;
    l->type = x;
    char16cpy(l->name, lobby_name.c_str(), 0x24);
    l->max_clients = 12;
    this->add_lobby(l);
  }
}

void ServerState::add_client_to_available_lobby(shared_ptr<Client> c) {
  auto it = this->id_to_lobby.lower_bound(0);
  for (; it != this->id_to_lobby.end(); it++) {
    if (!(it->second->flags & LobbyFlag::Public)) {
      continue;
    }
    try {
      it->second->add_client(c);
      break;
    } catch (const out_of_range&) { }
  }

  if (it == this->id_to_lobby.end()) {
    throw out_of_range("all lobbies full");
  }

  // send a join message to the joining player, and notifications to all others
  this->send_lobby_join_notifications(it->second, c);
}

void ServerState::remove_client_from_lobby(shared_ptr<Client> c) {
  auto l = this->id_to_lobby.at(c->lobby_id);
  l->remove_client(c);
  send_player_leave_notification(l, c->lobby_client_id);
}

void ServerState::change_client_lobby(shared_ptr<Client> c, shared_ptr<Lobby> new_lobby) {
  uint8_t old_lobby_client_id = c->lobby_client_id;

  shared_ptr<Lobby> current_lobby = this->find_lobby(c->lobby_id);
  try {
    if (current_lobby) {
      current_lobby->move_client_to_lobby(new_lobby, c);
    } else {
      new_lobby->add_client(c);
    }
  } catch (const out_of_range&) {
    send_lobby_message_box(c, u"$C6Can't change lobby\n\n$C7The lobby is full.");
    return;
  }

  if (current_lobby) {
    send_player_leave_notification(current_lobby, old_lobby_client_id);
    if (!(current_lobby->flags & LobbyFlag::Persistent) && (current_lobby->count_clients() == 0)) {
      this->remove_lobby(current_lobby->lobby_id);
    }
  }
  this->send_lobby_join_notifications(new_lobby, c);
}

void ServerState::send_lobby_join_notifications(shared_ptr<Lobby> l,
    shared_ptr<Client> joining_client) {
  for (auto& other_client : l->clients) {
    if (!other_client) {
      continue;
    } else if (other_client == joining_client) {
      send_join_lobby(joining_client, l);
    } else {
      send_player_join_notification(other_client, l, joining_client);
    }
  }
}

shared_ptr<Lobby> ServerState::find_lobby(uint32_t lobby_id) {
  return this->id_to_lobby.at(lobby_id);
}

vector<shared_ptr<Lobby>> ServerState::all_lobbies() {
  vector<shared_ptr<Lobby>> ret;
  for (auto& it : this->id_to_lobby) {
    ret.emplace_back(it.second);
  }
  return ret;
}

void ServerState::add_lobby(shared_ptr<Lobby> l) {
  l->lobby_id = this->next_lobby_id++;
  if (this->id_to_lobby.count(l->lobby_id)) {
    throw logic_error("lobby already exists with the given id");
  }
  this->id_to_lobby.emplace(l->lobby_id, l);
}

void ServerState::remove_lobby(uint32_t lobby_id) {
  this->id_to_lobby.erase(lobby_id);
}

shared_ptr<Client> ServerState::find_client(const char16_t* identifier,
    uint64_t serial_number, shared_ptr<Lobby> l) {

  if ((serial_number == 0) && identifier) {
    try {
      string encoded = encode_sjis(identifier);
      serial_number = stoull(encoded, NULL, 0);
    } catch (const exception&) { }
  }

  // look in the current lobby first
  if (l) {
    try {
      return l->find_client(identifier, serial_number);
    } catch (const out_of_range&) { }
  }

  // look in all lobbies if not found
  for (auto& other_l : this->all_lobbies()) {
    if (l == other_l) {
      continue; // don't bother looking again
    }
    try {
      return other_l->find_client(identifier, serial_number);
    } catch (const out_of_range&) { }
  }

  throw out_of_range("client not found");
}

uint32_t ServerState::connect_address_for_client(std::shared_ptr<Client> c) {
  // TODO: we can do something much smarter here, like use the sockname to find
  // out which interface the client is connected to, and return that address
  if (is_local_address(c->remote_addr)) {
    return this->local_address;
  } else {
    return this->external_address;
  }
}
