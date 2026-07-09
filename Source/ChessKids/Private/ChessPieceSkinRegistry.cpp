#include "ChessPieceSkinRegistry.h"

bool UChessPieceSkinRegistry::FindSkin(FName SkinId, FChessPieceSkinEntry& OutEntry) const
{
	for (const FChessPieceSkinEntry& Entry : Skins)
	{
		if (Entry.SkinId == SkinId)
		{
			OutEntry = Entry;
			return true;
		}
	}
	return false;
}
