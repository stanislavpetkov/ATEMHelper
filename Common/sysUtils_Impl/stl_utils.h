#pragma once
namespace pbn
{
	template< typename ContainerT, typename PredicateT >
	void erase_if(ContainerT& items, const PredicateT& predicate) {
		for (auto it = items.begin(); it != items.end(); ) {
			if (predicate(*it)) it = items.erase(it);
			else ++it;
		}
	}
}