// Fill out your copyright notice in the Description page of Project Settings.


#include "MPStream01GameInstance.h"
#include "ChunkDownloader.h"
#include "Json.h"
#include "Components/TextRenderComponent.h"
//#include "Actors/TextRenderActor.h"
#include "EggplantyverseDownloadableActor.h"

DEFINE_LOG_CATEGORY_STATIC(LogPatch, Display, Display);

void UMPStream01GameInstance::Init()
{
	Super::Init();


	InitPatching("Win64");


	/*
	//PB HACKY WAY FROM WEB, NOT USING BLUEPRINTS
	const FString DeploymentName = "Eggplantyverse-Test";
	const FString 
	= "Eggplantyverse-Test";

	// initialize the chunk downloader
	TSharedRef<FChunkDownloader> Downloader = FChunkDownloader::GetOrCreate();
	Downloader->Initialize("Windows", 8);

	// load the cached build ID
	Downloader->LoadCachedBuild(DeploymentName);

	// update the build manifest file
	TFunction<void(bool bSuccess)> UpdateCompleteCallback = [&](bool bSuccess) {bIsDownloadManifestUpToDate = bSuccess; };
	Downloader->UpdateBuild(DeploymentName, ContentBuildId, UpdateCompleteCallback);
	*/

}

void UMPStream01GameInstance::Shutdown()
{
	Super::Shutdown();

	// Shutdown the chunk downloader
	FChunkDownloader::Shutdown();
}

void UMPStream01GameInstance::InitPatching(const FString& VariantName)
{
	UE_LOG(LogPatch, Display, TEXT("PB VariantName = %s"), *VariantName);
	// save the patch platform
	PatchPlatform = VariantName;

	// get the HTTP module
	FHttpModule& Http = FHttpModule::Get();

	// create a new HTTP request and bind the response callback
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http.CreateRequest();
	Request->OnProcessRequestComplete().BindUObject(this, &UMPStream01GameInstance::OnPatchVersionResponse);

	//UE_LOG(LogPatch, Display, TEXT("Patch Content ID Response: %s"), *ContentBuildId);
	UE_LOG(LogPatch, Display, TEXT("PB PatchVersionURL = %s"), *PatchVersionURL);


	// configure and send the request
	Request->SetURL(PatchVersionURL);
	Request->SetVerb("GET");
	Request->SetHeader(TEXT("User-Agent"), "X-UnrealEngine-Agent");
	Request->SetHeader("Content-Type", TEXT("application/json"));
	Request->ProcessRequest();
}

void UMPStream01GameInstance::OnPatchVersionResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	UE_LOG(LogPatch, Display, TEXT("OnPatchVersionResponse"));

	// deployment name to use for this patcher configuration
	const FString DeploymentName = "Eggplantyverse-Test";

	// content build ID. Our HTTP response will provide this info
	// so we can update to new patch versions as they become available on the server
	//FString ContentBuildId = Response->GetContentAsString();

	//PB Forcing this to fix issue related to buildid
	FString ContentBuildId = "Eggplantyverse-Test";

	UE_LOG(LogPatch, Display, TEXT("Patch Content ID Response: %s"), *ContentBuildId);

	// ---------------------------------------------------------

	UE_LOG(LogPatch, Display, TEXT("Getting Chunk Downloader"));

	// initialize the chunk downloader with the chosen platform
	TSharedRef<FChunkDownloader> Downloader = FChunkDownloader::GetOrCreate();
	Downloader->Initialize(PatchPlatform, 8);

	UE_LOG(LogPatch, Display, TEXT("Loading Cached Build ID"));

	// Try to load the cached build manifest data if it exists.
	// This will populate the Chunk Downloader state with the most recently downloaded manifest data.
	if (Downloader->LoadCachedBuild(DeploymentName))
	{
		UE_LOG(LogPatch, Display, TEXT("Cached Build Succeeded"));
	}
	else {
		UE_LOG(LogPatch, Display, TEXT("Cached Build Failed"));
	}

	UE_LOG(LogPatch, Display, TEXT("Updating Build Manifest"));

	// Check with the server if there's a newer manifest file that matches our data, and download it.
	// ChunkDownloader uses a Lambda function to let us know when the async donwload is complete.
	TFunction<void(bool)> ManifestCompleteCallback = [this](bool bSuccess)
	{
		// write to the log
		if (bSuccess)
		{
			UE_LOG(LogPatch, Display, TEXT("Manifest Update Succeeded"));
		}
		else {
			UE_LOG(LogPatch, Display, TEXT("Manifest Update Failed"));
		}

		// Flag the manifest's status so we know whether it's safe to patch or not
		bIsDownloadManifestUpToDate = bSuccess;

		// call our delegate to let the game know we're ready to start patching
		OnPatchReady.Broadcast(bSuccess);


		//PB TESTING PATCHING RIGHT AWAY
		//PatchGame();

	};

	// Start the manifest update process, passing the BuildID and Lambda callback
	Downloader->UpdateBuild(DeploymentName, ContentBuildId, ManifestCompleteCallback);
}

bool UMPStream01GameInstance::PatchGame()
{
	UE_LOG(LogPatch, Display, TEXT("PB PatchGame"));

	// Make sure the download manifest is up to date
	if (!bIsDownloadManifestUpToDate)
	{
		// we couldn't contact the server to validate our manifest, so we can't patch
		UE_LOG(LogPatch, Display, TEXT("Manifest Update Failed. Can't patch the game"));

		return false;
	}
	UE_LOG(LogPatch, Display, TEXT("Starting the game patch process."));

	// get the chunk downloader
	TSharedRef<FChunkDownloader> Downloader = FChunkDownloader::GetChecked();

	// raise the patching flag
	bIsPatchingGame = true;

	// Write the current status for each chunk file from the manifest.
	// This way we can find out if a specific chunk is already downloaded, in progress, pending, etc.
	for (int32 ChunkID : ChunkDownloadList)
	{
		// print the chunk status to the log for debug purposes. We can do more complex logic here if we want.
		int32 ChunkStatus = static_cast<int32>(Downloader->GetChunkStatus(ChunkID));
		UE_LOG(LogPatch, Display, TEXT("Chunk %i status: %i"), ChunkID, ChunkStatus);
	}

	// prepare the pak mounting callback lambda function
	TFunction<void(bool)> MountCompleteCallback = [this](bool bSuccess)
	{
		// lower the chunk mount flag
		bIsPatchingGame = false;

		if (bSuccess)
		{
			UE_LOG(LogPatch, Display, TEXT("Chunk Mount complete"));

			// call the delegate
			OnPatchComplete.Broadcast(true);
		}
		else {
			UE_LOG(LogPatch, Display, TEXT("Chunk Mount failed"));

			// call the delegate
			OnPatchComplete.Broadcast(false);
		}
	};

	// mount the Paks so their assets can be accessed
	Downloader->MountChunks(ChunkDownloadList, MountCompleteCallback);

	// NOTE: This is not necessary, since MountChunk calls DownloadChunk under the hood. The Download Complete function is also unnecessary

	/*
	// Prepare the download Complete Lambda.
	// This will be called whenever the download process finishes.
	// We can use it to run further game logic on success/failure.
	TFunction<void(bool)> DownloadCompleteCallback = [this](bool bSuccess)
	{
		// For now, just write to the log
		if (bSuccess)
		{
			UE_LOG(LogPatch, Display, TEXT("Chunk load complete"));
		}
		else {
			UE_LOG(LogPatch, Display, TEXT("Chunk load failed"));
		}
	};

	// Start the chunk download process. ChunkDownloadList is filled manually by the user on the editor.
	Downloader->DownloadChunks(ChunkDownloadList, DownloadCompleteCallback, 1);

	// Set up the Loading Mode finished Lambda.
	// This will be called once chunk loading is complete,
	// and is a good way to know when the
	TFunction<void(bool)> LoadingModeCallback = [this](bool bSuccess)
	{
		OnDownloadComplete(bSuccess);
	};


	// Signal the Chunk Loader to switch to Loading Mode.
	// It will start keeping track of its loading stats, which we can query to update loading bars, etc.
	Downloader->BeginLoadingMode(LoadingModeCallback);
	*/

	return true;
}

FPPatchStats UMPStream01GameInstance::GetPatchStatus()
{
	//TODO: THIS IS CAUSING AN EXCEPTION FIX LATERRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRR
	FPPatchStats RetStats;
	//return RetStats;


	// get the chunk downloader
	TSharedRef<FChunkDownloader> Downloader = FChunkDownloader::GetChecked();

	// get the loading stats
	FChunkDownloader::FStats LoadingStats = Downloader->GetLoadingStats();

	// copy the info into the output stats.
	// ChunkDownloader tracks bytes downloaded as uint64s, which have no BP support,
	// So we divide to MB and cast to int32 (signed) to avoid overflow and interpretation errors.
	//FPPatchStats RetStats;

	RetStats.FilesDownloaded = LoadingStats.TotalFilesToDownload;
	RetStats.MBDownloaded = (int32)(LoadingStats.BytesDownloaded / (1024 * 1024));
	RetStats.TotalMBToDownload = (int32)(LoadingStats.TotalBytesToDownload / (1024 * 1024));
	RetStats.DownloadPercent = (float)LoadingStats.BytesDownloaded / (float)LoadingStats.TotalBytesToDownload;
	RetStats.LastError = LoadingStats.LastError;



	//UE_LOG(LogPatch, Display, TEXT("PB STATS TotalChunksToMount = %i"), RetStats.TotalChunksToMount);
	//UE_LOG(LogPatch, Display, TEXT("PB STATS FilesDownloaded = %i"), RetStats.FilesDownloaded);
	//UE_LOG(LogPatch, Display, TEXT("PB STATS LastError = %s"), RetStats.LastError);




	// return the status struct
	return RetStats;
}

void UMPStream01GameInstance::OnDownloadComplete(bool bSuccess)
{
	UE_LOG(LogPatch, Display, TEXT("OnDownloadComplete"));

	// was Pak download successful?
	if (!bSuccess)
	{
		UE_LOG(LogPatch, Display, TEXT("Patch Download Failed"));

		// call the delegate
		OnPatchComplete.Broadcast(false);

		return;
	}

	UE_LOG(LogPatch, Display, TEXT("Patch Download Complete2"));

	// get the chunk downloader
	TSharedRef<FChunkDownloader> Downloader = FChunkDownloader::GetChecked();

	// build the downloaded chunk list
	FJsonSerializableArrayInt DownloadedChunks;

	for (int32 ChunkID : ChunkDownloadList)
	{
		UE_LOG(LogPatch, Display, TEXT("Adding chunk %i ..."), ChunkID);
		DownloadedChunks.Add(ChunkID);
	}

	// prepare the pak mounting callback lambda function
	TFunction<void(bool)> MountCompleteCallback = [this](bool bSuccess)
	{
		// lower the chunk mount flag
		bIsPatchingGame = false;

		if (bSuccess)
		{
			UE_LOG(LogPatch, Display, TEXT("Chunk Mount complete"));

			// call the delegate
			OnPatchComplete.Broadcast(true);
		}
		else {
			UE_LOG(LogPatch, Display, TEXT("Chunk Mount failed"));

			// call the delegate
			OnPatchComplete.Broadcast(false);
		}
	};

	// mount the Paks so their assets can be accessed
	UE_LOG(LogPatch, Display, TEXT("Starting to mount chunks..."));
	Downloader->MountChunks(DownloadedChunks, MountCompleteCallback);
}

bool UMPStream01GameInstance::IsChunkLoaded(int32 ChunkID)
{
	// ensure our manifest is up to date
	if (!bIsDownloadManifestUpToDate)
		return false;

	// get the chunk downloader
	TSharedRef<FChunkDownloader> Downloader = FChunkDownloader::GetChecked();

	// query the chunk downloader for chunk status. Return true only if the chunk is mounted
	return Downloader->GetChunkStatus(ChunkID) == FChunkDownloader::EChunkStatus::Mounted;

}

bool UMPStream01GameInstance::DownloadSingleChunk(int32 ChunkID)
{
	TSharedRef<FChunkDownloader> Downloader = FChunkDownloader::GetOrCreate();

	UE_LOG(LogPatch, Display, TEXT("PB DownloadSingleChunk"));

	// Make sure the download manifest is up to date
	if (!bIsDownloadManifestUpToDate)
	{
		// we couldn't contact the server to validate our manifest, so we can't patch
		UE_LOG(LogPatch, Display, TEXT("Manifest Update Failed. Can't patch the game"));
		UE_LOG(LogPatch, Display, TEXT("TRYING TO DOWNLOAD CHUNK: %i"), ChunkID);

		return false;
	}

	// ignore individual downloads if we're still patching the game
	if (bIsPatchingGame)
	{
		UE_LOG(LogPatch, Display, TEXT("Main game patching underway. Ignoring single chunk downloads."));

		return false;
	}

	// make sure we're not trying to download the chunk multiple times
	if (SingleChunkDownloadList.Contains(ChunkID))
	{
		UE_LOG(LogPatch, Display, TEXT("Chunk: %i already downloading"), ChunkID);

		return false;
	}

	// raise the single chunk download flag
	bIsDownloadingSingleChunks = true;

	UE_LOG(LogPatch, Display, TEXT("PB Adding single chunk to DownloadList"));

	// add the chunk to our download list
	SingleChunkDownloadList.Add(ChunkID);

	UE_LOG(LogPatch, Display, TEXT("Downloading specific Chunk: %i"), ChunkID);

	// get the chunk downloader
	//TSharedRef<FChunkDownloader> Downloader = FChunkDownloader::GetChecked();

	// prepare the pak mounting callback lambda function
	TFunction<void(bool)> MountCompleteCallback = [this](bool bSuccess)
	{
		UE_LOG(LogPatch, Display, TEXT("PB MountCompleteCallback"));

		if (bSuccess)
		{
			UE_LOG(LogPatch, Display, TEXT("Single Chunk Mount complete"));

			// call the delegate
			OnSingleChunkPatchComplete.Broadcast(true);

			UE_LOG(LogPatch, Display, TEXT("PB Adding OnPatchComplete broadcast"));
			OnPatchComplete.Broadcast(true);
		}
		else {
			UE_LOG(LogPatch, Display, TEXT("Single Mount failed"));

			// call the delegate
			OnSingleChunkPatchComplete.Broadcast(false);
		}
	};

	UE_LOG(LogPatch, Display, TEXT("PB Downloader->MountChunk %i"), ChunkID);

	// mount the Paks so their assets can be accessed
	//Downloader->MountChunk(ChunkID, MountCompleteCallback);
	////////////////////////////////////////////////////////////////////////////////////////
	////PB ->MountChunk FAILS, SO WE ARE TRYING DownloadChunks , BeginLoadingMode




	////////////////////////////////////////////////////////////////////////////////////////
	////PB MountChunk FAILS, SO WE ARE TRYING MountChunks instead
	//UE_LOG(LogPatch, Display, TEXT("*****************************************************"));
	//UE_LOG(LogPatch, Display, TEXT("PB ->MountChunk FAILS, SO WE ARE TRYING ->MountChunks instead"));

	//FJsonSerializableArrayInt DownloadedChunks;

	//for (int32 ChunkID : ChunkDownloadList)
	//{
	//	UE_LOG(LogPatch, Display, TEXT("PB Adding chunk %i ..."), ChunkID);
	//	DownloadedChunks.Add(ChunkID);
	//}

	//Downloader->MountChunks(DownloadedChunks, MountCompleteCallback);
	////////////////////////////////////////////////////////////////////////////////////////





	// NOTE: This is not necessary, since MountChunk calls DownloadChunk under the hood. The Download Complete function is also unnecessary

	
	// Prepare the download Complete Lambda.
	// This will be called whenever the download process finishes.
	// We can use it to run further game logic on success/failure.
	TFunction<void(bool)> DownloadCompleteCallback = [this](bool bSuccess)
	{
		// For now, just write to the log
		if (bSuccess)
		{
			UE_LOG(LogPatch, Display, TEXT("Specific Chunk load complete"));
		}
		else {
			UE_LOG(LogPatch, Display, TEXT("Specific Chunk load failed"));
		}
	};

	Downloader->DownloadChunk(ChunkID, DownloadCompleteCallback, 1);

	// Set up the Loading Mode finished Lambda.
	TFunction<void(bool)> LoadingModeCallback = [this](bool bSuccess)
	{
		UE_LOG(LogPatch, Display, TEXT("PB LoadingModeCallback"));

		OnSingleChunkDownloadComplete(bSuccess);
	};

	// Signal the Chunk Loader to switch to Loading Mode.
	// It will start keeping track of its loading stats, which we can query to update loading bars, etc.
	Downloader->BeginLoadingMode(LoadingModeCallback);
	

	return true;

}

void UMPStream01GameInstance::OnSingleChunkDownloadComplete(bool bSuccess)
{
	UE_LOG(LogPatch, Display, TEXT("OnSingleChunkDownloadComplete"));

	TSharedRef<FChunkDownloader> Downloader = FChunkDownloader::GetOrCreate();

	// NOTE: disabled to repro the crash

	// skip if we weren't in download mode.
	// this is necessary because the download complete callback
	// is still valid here, and MountChunks() may cause it to trigger again.
	// this is most likely an error and should be patched in future versions.
	//if(!bIsDownloadingSingleChunks)
	//{
	//	return;
	//}

	// NOTE: end repro

	// lower the download chunks flag
	bIsDownloadingSingleChunks = false;

	// was Pak download successful?
	if (!bSuccess)
	{
		UE_LOG(LogPatch, Display, TEXT("Patch Download Failed"));

		// call the delegate
		OnSingleChunkPatchComplete.Broadcast(false);

		return;
	}

	UE_LOG(LogPatch, Display, TEXT("Patch Download Complete1"));

	// get the chunk downloader
	//TSharedRef<FChunkDownloader> Downloader = FChunkDownloader::GetChecked();

	// build the downloaded chunk list
	FJsonSerializableArrayInt DownloadedChunks;

	for (int32 ChunkID : SingleChunkDownloadList)
	{
		DownloadedChunks.Add(ChunkID);
	}

	// NOTE: commented to repro the crash

	// empty the chunk download list
	//SingleChunkDownloadList.Empty();

	// NOTE: end repro

	// prepare the pak mounting callback lambda function
	TFunction<void(bool)> MountCompleteCallback = [this](bool bSuccess)
	{
		if (bSuccess)
		{
			UE_LOG(LogPatch, Display, TEXT("Single Chunk Mount complete"));

			// call the delegate
			OnSingleChunkPatchComplete.Broadcast(true);
		}
		else {
			UE_LOG(LogPatch, Display, TEXT("Single Mount failed"));

			// call the delegate
			OnSingleChunkPatchComplete.Broadcast(false);
		}
	};

	// mount the Paks so their assets can be accessed
	Downloader->MountChunks(DownloadedChunks, MountCompleteCallback);

}




void UMPStream01GameInstance::OnResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		GLog->Log("Hey we received the following response!");
		//FString ResponseString = Response->GetContentAsString();
		FString JsonString = Response->GetContentAsString();
		GLog->Log(JsonString);


		//Create a json object to store the information from the json string
		//The json reader is used to deserialize the json object later on
		TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());
		TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(JsonString);

		if (FJsonSerializer::Deserialize(JsonReader, JsonObject) && JsonObject.IsValid())
		{
			TArray<TSharedPtr<FJsonValue>> ResponseObject = JsonObject->GetArrayField("response");
			
			int num_asssets = ResponseObject.Num();
			UE_LOG(LogPatch, Display, TEXT("num_asssets %i"), num_asssets);

			for (int i = 0; i < num_asssets; i++){
			//	//GLog->Log("interating num %i", num_asssets);
				UE_LOG(LogPatch, Display, TEXT("interating num %i"), i);
				//TSharedPtr<FJsonObject> Record = ResponseObject[i]->GetObjectField("Record");
				
				
				TSharedPtr<FJsonObject> Item = ResponseObject[i]->AsObject();
				//TSharedPtr<FJsonObject> Record = ResponseObject[i]->AsObject();
				TSharedPtr<FJsonObject> Record = Item->GetObjectField("Record");
				FString Asset = Record->GetStringField("Asset");
				FString CoordX = Record->GetStringField("CoordX");
				FString Coordy = Record->GetStringField("Coordy");
				FString Coordz = Record->GetStringField("Coordz");
				FString ID = Record->GetStringField("ID");
				FString Owner = Record->GetStringField("Owner");
				FString Type = Record->GetStringField("Type");

				float coord_x = FCString::Atof(*CoordX);
				float coord_y = FCString::Atof(*Coordy);
				float coord_z = FCString::Atof(*Coordz);

				UE_LOG(LogPatch, Display, TEXT("Owner = %s"), *Owner);
/*
			"Record": {
				"Asset": "BP_EggplantyverseCube",
				"CoordX": "-390",
				"Coordy": "280",
				"Coordz": "325",
				"ID": "pablotest1",
				"Owner": "Pablo",
				"Type": "nft"
			}*/



				UWorld* const World = GetWorld(); // get a reference to the world
				if (World) {
					// if world exists
					//FVector SpawnLocation = FVector(-390, 280, 325);
					FVector SpawnLocation = FVector(coord_x, coord_y, coord_z);
					FRotator SpawnRotation = FRotator(0, 0, 0);

					//FStringClassReference strClassRef("Blueprint'/Game/AssetTest/BP_EggplantyverseCube.BP_EggplantyverseCube_C'");
					//FStringClassReference strClassRef("Blueprint'/Game/AssetTest2/BP_EggplantyverseCone.BP_EggplantyverseCone_C'");
					//FString ClassRef = "Blueprint'/Game/AssetTest2/" + Asset + "." + Asset + "_C'";
					FString ClassRef = "Blueprint'/Game/" + Asset + "/" + Asset + "." + Asset + "_C'";

					UE_LOG(LogPatch, Display, TEXT("ClassRef = %s"), *ClassRef);

					FStringClassReference strClassRef(ClassRef);

					//strClassRef.TryLoadClass<AActor>();
					//UClass* EggplantyverseAssetToSpawn = nullptr;
					//EggplantyverseAssetToSpawn = strClassRef.ResolveClass();
					//World->SpawnActor<AActor>(EggplantyverseAssetToSpawn, SpawnLocation, SpawnRotation);

					strClassRef.TryLoadClass<AEggplantyverseDownloadableActor>();
					UClass* EggplantyverseAssetToSpawn = nullptr;
					EggplantyverseAssetToSpawn = strClassRef.ResolveClass();
					AEggplantyverseDownloadableActor *spawnedAsset = World->SpawnActor<AEggplantyverseDownloadableActor>(EggplantyverseAssetToSpawn, SpawnLocation, SpawnRotation);
					spawnedAsset->EggplantyverseUsername = Owner;
					spawnedAsset->EggplantyverseID = ID;
					spawnedAsset->OnEggplantyverseRefresh.Broadcast();

				}
			}
		}
		else
		{
			GLog->Log("couldn't deserialize");
		}

	}
}

//void UMPStream01GameInstance::SendHttpRequest(const FString& Url, const FString& RequestContent, const FString& Verb)
void UMPStream01GameInstance::SendHttpRequest(const FString& Url, const FString& RequestContent, const FString& Verb)
{
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url);
	
	Request->SetHeader(TEXT("User-Agent"), "X-UnrealEngine-Agent");

	if (Verb == "POST") {
		Request->SetVerb("POST");
	}
	else {
		Request->SetVerb("GET");
		//Binding a function that will fire when we receive a response from our request
		Request->OnProcessRequestComplete().BindUObject(this, &UMPStream01GameInstance::OnResponseReceived);
	}

	//Tell the host that we're an unreal agent

	//In case you're sending json you can also make use of headers using the following command. 
		//Just make sure to convert the Json to string and use the SetContentAsString.
	//Request->SetHeader("Content-Type", TEXT("application/json"));
	//Use the following command in case you want to send a string value to the server
	//Request->SetContentAsString("Hello kind server.");

	//Send the request
	Request->ProcessRequest();
}

//void UMPStream01GameInstance::SendHttpRequest(const FString& Url, const FString& RequestContent, const FString& Verb)
void UMPStream01GameInstance::SendPostHttpRequest(const FString& Url, const FString& RequestContent,
	const FString& Asset, const FString& Owner, const FString& Type, float X, float Y, float Z)
{
	//Creating a request using UE4's Http Module
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();

	UE_LOG(LogTemp, Warning, TEXT("Calling URL = %s"), *Url);

	//This is the url on which to process the request
	Request->SetURL(Url);
	//We're sending data so set the Http method to POST
	//Request->SetVerb("POST");

	Request->SetHeader(TEXT("User-Agent"), "X-UnrealEngine-Agent");

	Request->SetVerb("POST");
	//if (Verb=="POST") {
	//	
	//}
	//else {
	//	Request->SetVerb("GET");
	//	//Binding a function that will fire when we receive a response from our request
	//	Request->OnProcessRequestComplete().BindUObject(this, &UMPStream01GameInstance::OnResponseReceived); 
	//}


	//int AmountOfCharacters = 32;
	//FString RandomId = "";

	//for (int i = 0; i < AmountOfCharacters; ++i) {
	//	//RandomId += FMath::RandRange(32, 127);
	//	RandomId.Append(FMath::RandRange(32, 127));
	//}
	FGuid MyGuid = FGuid::NewGuid();
	FString RandomId = MyGuid.ToString();

	UE_LOG(LogTemp, Warning, TEXT("Random Guid = %s"), *RandomId);

	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);

	JsonObject->SetStringField("id", RandomId);
	JsonObject->SetStringField("type", "nft");
	//JsonObject->SetStringField("ast", "BP_EggplantyverseCube");
	JsonObject->SetStringField("ast", Asset);
	//JsonObject->SetStringField("owner", "Pablo");
	JsonObject->SetStringField("owner", EggplantyverseUsername);
	JsonObject->SetNumberField("x", X);
	JsonObject->SetNumberField("y", Y);
	JsonObject->SetNumberField("z", Z);

	//date values that will be used in the future
	//JsonObject->SetStringField("dm", "");
	//JsonObject->SetStringField("dc", ""); 
	JsonObject->SetStringField("dm", FDateTime::Now().ToString());
	JsonObject->SetStringField("dc", FDateTime::Now().ToString());


	FString OutputString;
	TSharedRef< TJsonWriter<> > Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

	UE_LOG(LogTemp, Warning, TEXT("resulting jsonString -> %s"), *OutputString);
	

	///////////////////////////////////////////////////////////////////////
	//PB GET VALUE FROM BLUEPRINT
	//static const FName MyProperty(TEXT("MyInt32Variable"));

	//UClass* MyClass = GetClass();

	//for (UProperty* Property = MyClass->PropertyLink; Property; Property = Property->PropertyLinkNext)
	//{
	//	UIntProperty* IntProperty = Cast<UIntProperty>(Property);
	//	if (IntProperty && Property->GetFName() == MyProperty)
	//	{
	//		// Need more work for arrays
	//		int32 MyIntValue = IntProperty->GetPropertyValue(Property->ContainerPtrToValuePtr<int32>(this));

	//		UE_LOG(LogPB, Log, TEXT("Value is = %i"), MyIntValue);
	//		break;
	//	}
	//}
	///////////////////////////////////////////////////////////////////////




	//Tell the host that we're an unreal agent
	

	//In case you're sending json you can also make use of headers using the following command. 
		//Just make sure to convert the Json to string and use the SetContentAsString.
	Request->SetHeader("Content-Type", TEXT("application/json"));
	//Use the following command in case you want to send a string value to the server
	Request->SetContentAsString(OutputString);

	//Send the request
	Request->ProcessRequest();
}



//void UMPStream01GameInstance::ParseExample(const FString& JsonRaw)
////void UMPStream01GameInstance::ParseExample(FString JsonRaw)
//{
//	//FString JsonRaw = "{ \"exampleString\": \"Hello World\" }";
//	TSharedPtr<FJsonObject> JsonParsed;
//	TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(JsonRaw);
//
//	if (FJsonSerializer::Deserialize(JsonReader, JsonParsed))
//	{
//		FString ExampleString = JsonParsed->GetStringField("exampleString");
//	}
//}


