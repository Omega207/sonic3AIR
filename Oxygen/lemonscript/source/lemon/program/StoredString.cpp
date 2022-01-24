/*
*	Part of the Oxygen Engine / Sonic 3 A.I.R. software distribution.
*	Copyright (C) 2017-2022 by Eukaryot
*
*	Published under the GNU GPLv3 open source software license, see license.txt
*	or https://www.gnu.org/licenses/gpl-3.0.en.html
*/

#include "lemon/pch.h"
#include "lemon/program/StoredString.h"


namespace lemon
{
	void StringLookup::clear()
	{
		for (size_t k = 0; k < HASH_TABLE_SIZE; ++k)
		{
			TableEntry* entry = mTable[k];
			while (nullptr != entry)
			{
				TableEntry* next = entry->mNext;
				mEntryPool.destroyObject(*entry);
				entry = next;
			}
			mTable[k] = nullptr;
		}
		mEntryPool.clear();
		mNumEntries = 0;
	}

	const StoredString* StringLookup::getStringByHash(uint64 hash) const
	{
		const TableEntry* entry = mTable[hash & HASH_TABLE_BITMASK];
		while (nullptr != entry)
		{
			if (entry->mStoredString.mHash == hash)
			{
				// Entry exists
				return &entry->mStoredString;
			}
			entry = entry->mNext;
		}
		return nullptr;
	}

	const StoredString& StringLookup::getOrAddString(const std::string& str)
	{
		return getOrAddString(str, rmx::getMurmur2_64(str));
	}

	const StoredString& StringLookup::getOrAddString(const std::string& str, uint64 hash)
	{
		TableEntry** ptr = &mTable[hash & HASH_TABLE_BITMASK];
		while (nullptr != *ptr)
		{
			if ((*ptr)->mStoredString.mHash == hash)
			{
				// It's already added
				return (*ptr)->mStoredString;
			}
			ptr = &(*ptr)->mNext;
		}

		// Add new string
		TableEntry& newEntry = mEntryPool.createObject();
		newEntry.mStoredString.mString = str;
		newEntry.mStoredString.mHash = hash;
		*ptr = &newEntry;
		++mNumEntries;
		return newEntry.mStoredString;
	}

	const StoredString& StringLookup::getOrAddString(const char* str, size_t length)
	{
		const uint64 hash = rmx::getMurmur2_64((const uint8*)str, length);
		TableEntry** ptr = &mTable[hash & HASH_TABLE_BITMASK];
		while (nullptr != *ptr)
		{
			if ((*ptr)->mStoredString.mHash == hash)
			{
				// It's already added
				return (*ptr)->mStoredString;
			}
			ptr = &(*ptr)->mNext;
		}

		// Add new string
		TableEntry& newEntry = mEntryPool.createObject();
		newEntry.mStoredString.mString.append(str, length);
		newEntry.mStoredString.mHash = hash;
		*ptr = &newEntry;
		++mNumEntries;
		return newEntry.mStoredString;
	}

	void StringLookup::addFromLookup(const StringLookup& other)
	{
		for (int k = 0; k < HASH_TABLE_SIZE; ++k)
		{
			TableEntry* sourceEntry = other.mTable[k];
			while (nullptr != sourceEntry)
			{
				getOrAddString(sourceEntry->mStoredString.getString(), sourceEntry->mStoredString.getHash());
				sourceEntry = sourceEntry->mNext;
			}
		}
	}

	void StringLookup::serialize(VectorBinarySerializer& serializer)
	{
		uint32 numberOfEntries = (uint32)mNumEntries;
		serializer & numberOfEntries;

		if (serializer.isReading())
		{
			for (uint32 i = 0; i < numberOfEntries; ++i)
			{
				const uint64 hash = serializer.read<uint64>();
				const std::string str = serializer.read<std::string>();
				getOrAddString(str, hash);
			}
		}
		else
		{
			for (int k = 0; k < HASH_TABLE_SIZE; ++k)
			{
				TableEntry* entry = mTable[k];
				while (nullptr != entry)
				{
					serializer.write(entry->mStoredString.getHash());
					serializer.write(entry->mStoredString.getString());
					entry = entry->mNext;
				}
			}
		}
	}
}
