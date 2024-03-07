#pragma once

#include "../libs.hpp"
#include "connection/connection.hpp"

// Initialise work dir and create connection with server
std::filesystem::path init(struct data::server&, struct data::client&);

// Creates a new directory
void create_dir(std::string&);
