#include"all_defines.hpp"

#include"symbols.hpp"
#include"processes.hpp"
#include"mutexes.hpp"

#include<boost/noncopyable.hpp>

#include<string>
#include<map>

void Symbol::copy_value_to(ValueHolderRef& p) {
	{AppLock l(m);
		/*Unfortunately the entire cloning has to be
		done while we have the lock.  This is because
		a Symbol::set_value() might invalidate the
		pointer from under us if we didn't do the
		entire cloning locked.
		Other alternatives exist: we can use a locked
		reference counting scheme, or use some sort
		of deferred deallocation.
		*/
		value->clone(p);
	}
}
void Symbol::copy_value_to_and_add_notify(ValueHolderRef& p, Process* R) {
	{AppLock l(m);
	  if (value.empty()) {
			// no value associated with this symbol
			show_HlError(R->stack, ("unbound variable: " + printname).c_str());
			throw_HlError("unbound variable");
		}
		value->clone(p);
		/*check for dead processes in notification list*/
		size_t j = 0;
		for(size_t i = 0; i < notification_list.size(); ++i) {
			if(!notification_list[i]->is_dead()) {
				if(i != j) {
					notification_list[j] = notification_list[i];
				}
				++j;
			}
		}
		notification_list.resize(j + 1);
		notification_list[j] = R;
	}
}

void Symbol::set_value(Object::ref o) {
	ValueHolderRef tmp;
	ValueHolder::copy_object(tmp, o);
	{AppLock l(m);
		value.swap(tmp);
		size_t j = 0;
		for(size_t i = 0; i < notification_list.size(); ++i) {
			if(!notification_list[i]->is_dead()) {
				if(i != j) {
					notification_list[j] = notification_list[i];
				}
				notification_list[j]->notify_global_change(this);
				++j;
			}
		}
		notification_list.resize(j);
	}
}

void Symbol::clean_notification_list(std::set<Process*> const& dead) {
	size_t j = 0;
	for(size_t i = 0; i < notification_list.size(); ++i) {
		/*not counted among the dead?*/
		if(dead.count(notification_list[i]) == 0) {
			if(i != j) {
				notification_list[j] = notification_list[i];
			}
			++j;
		}
	}
	notification_list.resize(j);
}


