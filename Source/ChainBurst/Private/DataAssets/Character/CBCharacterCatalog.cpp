// project
#include "DataAssets/Character/CBCharacterCatalog.h"
#include "Characters/CBChaserCharacter.h" // TSoftClassPtr::LoadSynchronous 가 완전한 타입을 요구함

bool FCBCharacterEntry::IsValid() const
{
	// 태그가 없으면 조회할 키가 없고, 클래스가 없으면 스폰할 것이 없음
	return CharacterId.IsValid() && !CharacterClass.IsNull();
}

const FCBCharacterEntry* UCBCharacterCatalog::FindEntry(const FGameplayTag& InCharacterId) const
{
	if (!InCharacterId.IsValid()) return nullptr;

	// 태그가 일치하는 항목을 찾아 반환. 없으면 nullptr
	return Entries.FindByPredicate(
		[&InCharacterId](const FCBCharacterEntry& InEntry) { return InEntry.CharacterId == InCharacterId; });
}

bool UCBCharacterCatalog::IsValidCharacterId(const FGameplayTag& InCharacterId) const
{
	const FCBCharacterEntry* FoundEntry = FindEntry(InCharacterId);

	// 등록돼 있고 설정이 온전해야 통과
	return FoundEntry && FoundEntry->IsValid();
}

UClass* UCBCharacterCatalog::LoadCharacterClass(const FGameplayTag& InCharacterId) const
{
	const FCBCharacterEntry* FoundEntry = FindEntry(InCharacterId);
	if (!FoundEntry || !FoundEntry->IsValid()) return nullptr;

	// 이미 로드돼 있으면 그대로 반환, 아니면 동기 로드.
	// 로비 게임 스테이트가 미리 로드해 두므로 보통은 즉시 반환됨
	return FoundEntry->CharacterClass.LoadSynchronous();
}

void UCBCharacterCatalog::GetCharacterClassPaths(TArray<FSoftObjectPath>& OutClassPaths) const
{
	OutClassPaths.Reset();

	for (const FCBCharacterEntry& Entry : Entries)
	{
		if (!Entry.IsValid()) continue;

		OutClassPaths.Add(Entry.CharacterClass.ToSoftObjectPath());
	}
}

void UCBCharacterCatalog::GetAllEntries(TArray<FCBCharacterEntry>& OutEntries) const
{
	OutEntries.Reset();

	for (const FCBCharacterEntry& Entry : Entries)
	{
		if (!Entry.IsValid()) continue;

		OutEntries.Add(Entry);
	}
}
