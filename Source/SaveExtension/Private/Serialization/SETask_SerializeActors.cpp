// Copyright 2015-2020 Piperift. All Rights Reserved.

#include "Serialization/SETask_SerializeActors.h"
#include "Serialization/MemoryWriter.h"
#include "Components/PrimitiveComponent.h"
#include "SESaveManager.h"
#include "SESlotData.h"
#include "SESavePreset.h"
#include "Serialization/SEArchive.h"


/////////////////////////////////////////////////////
// FMTTask_SerializeActors
void FSETask_SerializeActors::DoWork()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FSETask_SerializeActors::DoWork);
	if (bStoreGameInstance)
	{
		SerializeGameInstance();
	}

	for (int32 I = 0; I < Num; ++I)
	{
		const AActor* const Actor = (*LevelActors)[StartIndex + I];
		if (Actor && Filter.ShouldSave(Actor))
		{
			FSEActorRecord& Record = ActorRecords.AddDefaulted_GetRef();
			SerializeActor(Actor, Record);
		}
	}
}

void FSETask_SerializeActors::SerializeGameInstance()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FMTTask_SerializeActors::SerializeGameInstance);
	if (UGameInstance* GameInstance = World->GetGameInstance())
	{
		FSEObjectRecord Record{ GameInstance };

		//Serialize into Record Data
		FMemoryWriter MemoryWriter(Record.Data, true);
		FSEArchive Archive(MemoryWriter, false);
		GameInstance->Serialize(Archive);

		SlotData->GameInstance = MoveTemp(Record);
	}
}

bool FSETask_SerializeActors::SerializeActor(const AActor* Actor, FSEActorRecord& Record) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FMTTask_SerializeActors::SerializeActor);

	//Clean the record
	Record = { Actor };

	Record.bHiddenInGame = Actor->IsHidden();
	Record.bIsProcedural = Filter.IsProcedural(Actor);

	if (Filter.StoresTags(Actor))
	{
		Record.Tags = Actor->Tags;
	}
	else
	{
		// Only save save-tags
		for (const auto& Tag : Actor->Tags)
		{
			if (Filter.IsSaveTag(Tag))
			{
				Record.Tags.Add(Tag);
			}
		}
	}

	if (Filter.StoresTransform(Actor))
	{
		Record.Transform = Actor->GetTransform();

		if (Filter.StoresPhysics(Actor))
		{
			USceneComponent* const Root = Actor->GetRootComponent();
			if (Root && Root->Mobility == EComponentMobility::Movable)
			{
				if (auto* const Primitive = Cast<UPrimitiveComponent>(Root))
				{
					Record.LinearVelocity = Primitive->GetPhysicsLinearVelocity();
					Record.AngularVelocity = Primitive->GetPhysicsAngularVelocityInRadians();
				}
				else
				{
					Record.LinearVelocity = Root->GetComponentVelocity();
				}
			}
		}
	}

	if (Filter.bStoreComponents)
	{
		SerializeActorComponents(Actor, Record, 1);
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(Serialize);
	FMemoryWriter MemoryWriter(Record.Data, true);
	FSEArchive Archive(MemoryWriter, false);
	const_cast<AActor*>(Actor)->Serialize(Archive);

	return true;
}

void FSETask_SerializeActors::SerializeActorComponents(const AActor* Actor, FSEActorRecord& ActorRecord, int8 Indent /*= 0*/) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FMTTask_SerializeActors::SerializeActorComponents);

	const TSet<UActorComponent*>& Components = Actor->GetComponents();
	for (auto* Component : Components)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FMTTask_SerializeActors::SerializeActorComponents|Component);
		if (Filter.ShouldSave(Component))
		{
			FSEComponentRecord ComponentRecord;
			ComponentRecord.Name = Component->GetFName();
			ComponentRecord.Class = Component->GetClass();

			if (Filter.StoresTransform(Component))
			{
				const USceneComponent* Scene = CastChecked<USceneComponent>(Component);
				if (Scene->Mobility == EComponentMobility::Movable)
				{
					ComponentRecord.Transform = Scene->GetRelativeTransform();
				}
			}

			if (Filter.StoresTags(Component))
			{
				ComponentRecord.Tags = Component->ComponentTags;
			}

			if (!Component->GetClass()->IsChildOf<UPrimitiveComponent>())
			{
				FMemoryWriter MemoryWriter(ComponentRecord.Data, true);
				FSEArchive Archive(MemoryWriter, false);
				Component->Serialize(Archive);
			}
			ActorRecord.ComponentRecords.Add(ComponentRecord);
		}
	}
}
