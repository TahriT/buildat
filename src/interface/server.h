#pragma once
#include "core/types.h"
#include "interface/event.h"

namespace interface
{
	struct Module;

	struct Server
	{
		virtual ~Server(){}
		virtual void load_module(const ss_ &module_name, const ss_ &path) = 0;
		virtual ss_ get_modules_path() = 0;
		virtual ss_ get_builtin_modules_path() = 0;
		virtual bool has_module(const ss_ &module_name) = 0;
		virtual void sub_event(struct Module *module, const Event::Type &type) = 0;
		virtual void emit_event(const Event &event) = 0;
	};
}
