// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
// Copyright 1998-2016 Bright Night Games, Inc. All Rights Reserved.
#include "JsonFieldData.h"

USerializableInterface::USerializableInterface(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

//////////////////////////////////////////////////////////////////////////
// UJsonFieldData

/**
* Constructor
*/
UJsonFieldData::UJsonFieldData(const class FObjectInitializer& PCIP)
	: Super(PCIP) 
{
	Reset();
}

/**
* Resets the current page data
*
*/
void UJsonFieldData::Reset() {
	// If the post data is valid
	if (Data.IsValid()) {
		// Clear the current post data
		Data.Reset();
	}

	// Create a new JSON object
	Data = MakeShareable(new FJsonObject());
}

/**
* Serialize the JSON to a String
*
* @return	A string representation of the json content
*/
FString UJsonFieldData::GetContentString()
{
	FString outStr;
	TSharedRef<TJsonWriter<TCHAR>> JsonWriter = TJsonWriterFactory<TCHAR>::Create(&outStr);

	// Start writing the response
	WriteObject(JsonWriter, "", new FJsonValueObject(Data));
	JsonWriter->Close();

	return outStr;
}

/**
* Serialize the JSON to a compressed format
* 
* @param	Compressed	An array to store the result
* @param	bIsValid	The status of the compression
*
*/
 void UJsonFieldData::GetContentCompressed(TArray<uint8>& Compressed, bool& bIsValid)
{
	FString UncompressedData = GetContentString();
	uint32 UncompressedSize = sizeof(TCHAR)*(UncompressedData.Len());

	int32 CompressedSize = FCompression::CompressMemoryBound(COMPRESS_ZLIB, UncompressedSize);
	TArray<uint8> CompressedData;
	CompressedData.AddUninitialized(CompressedSize + sizeof(UncompressedSize));
	
	FMemory::Memcpy(&CompressedData[0], &UncompressedSize, sizeof(UncompressedSize));
	bIsValid = FCompression::CompressMemory(COMPRESS_ZLIB, CompressedData.GetData() + sizeof(UncompressedSize), CompressedSize, *UncompressedData, UncompressedSize);
	
	if (bIsValid) {
		CompressedData.SetNum(CompressedSize+ sizeof(UncompressedSize), false);

		Compressed = CompressedData;
	}
}

/**
* Create a new instance of the UJsonFieldData class, for use in Blueprint graphs.
*
* @param	WorldContextObject		The current context
*
* @return	A pointer to the newly created post data
*/
UJsonFieldData* UJsonFieldData::Create(UObject* WorldContextObject) {
	// Get the world object from the context
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);

	// Construct the object and return it
	UJsonFieldData* fieldData = NewObject<UJsonFieldData>(WorldContextObject);
	fieldData->contextObject = WorldContextObject;
	return fieldData;
}

/**
* This function will write the supplied key and value to the JsonWriter
*
* @param	writer			The JsonWriter to use
* @param	key				Object key
* @param	value			Object value
*
*/
void UJsonFieldData::WriteObject(TSharedRef<TJsonWriter<TCHAR>> writer, FString key, FJsonValue* value) {
	if (value->Type == EJson::String) {
		// Write simple string entry, don't a key when it isn't set
		if (key.Len() > 0) {
			writer->WriteValue(key, value->AsString());
		}
		else {
			writer->WriteValue(value->AsString());
		}
	}
	else if (value->Type == EJson::Number) {
		// Write simple string entry, don't a key when it isn't set
		if (key.Len() > 0) {
			writer->WriteValue(key, value->AsNumber());
		}
		else {
			writer->WriteValue(value->AsNumber());
		}
	}
	else if (value->Type == EJson::Object) {
		// Write object entry
		if (key.Len() > 0) {
			writer->WriteObjectStart(key); }
		else {
			writer->WriteObjectStart(); }

		// Loop through all the values in the object data
		TSharedPtr<FJsonObject> objectData = value->AsObject();
		for (auto objectValue = objectData->Values.CreateIterator(); objectValue; ++objectValue) {
			// Using recursion to write the key and value to the writer
			WriteObject(writer, objectValue.Key(), objectValue.Value().Get());
		}

		writer->WriteObjectEnd();
	} else if (value->Type == EJson::Array) {
		// Process array entry
		writer->WriteArrayStart(key);
			
		TArray<TSharedPtr<FJsonValue>> objectArray = value->AsArray();
		for (int32 i = 0; i < objectArray.Num(); i++) {
			// Use recursion with an empty key to process all the values in the array
			WriteObject(writer, "", objectArray[i].Get());
		}

		writer->WriteArrayEnd();
	}
}

/**
* Adds the supplied string to the post data, under the given key
*
* @param	key						Key
* @param	value					Object value
*
* @return	The object itself
*/
UJsonFieldData* UJsonFieldData::SetString(const FString& key, const FString& value) {
	Data->SetStringField(*key,*value);
	return this;
}

/**
* Adds the supplied object to the post data, under the given key
*
* @param	key						Key
* @param	objectData				Object data
*
* @return	The object itself
*/
UJsonFieldData* UJsonFieldData::SetObject(const FString& key, const UJsonFieldData* objectData) {
	Data->SetObjectField(*key, objectData->Data);
	return this;
}


/**
* Adds the supplied UObject to the post data, under the given key
*
* @param	key					Key
* @param	value				UObject
*
* @return	The object itself
*/
UJsonFieldData* UJsonFieldData::SetUObject(const FString& key, const UObject* Container)
{

	if (!Container) {
		UE_LOG(LogJson, Error, TEXT("NULL Object Reference for %s"), *key);
		return this;
	}
	UClass* ObjectClass = Container->GetClass();

	//UObject* Value = Container->GetPropertyValue(Container->ContainerPtrToValuePtr<UObject>(Container));
	TSharedPtr<FJsonObject> JsonObject = CreateJsonValueFromUObject(Container);
	Data->SetObjectField(*key, JsonObject);

	return this;
	
	
	//for (UProperty* Property = ObjectClass->PropertyLink; Property; Property = Property->PropertyLinkNext)
	//{
	//	//TOTO Use > bool UProperty::ShouldSerializeValue( FArchive& Ar ) const
	//	if (!Property->HasAllPropertyFlags(
	//		EPropertyFlags::CPF_SaveGame
	//	)) continue;

	//	const FString PropertyName = Property->GetFName().ToString();

	//	WriteProperty(JsonObject.Get(), Property, Container);

		/*

		if (auto objectProperty = Cast<UObjectProperty>(Property))
		{
			UObject* Value = objectProperty->GetPropertyValue(objectProperty->ContainerPtrToValuePtr<UObject>(Container));
			if (Value)
			{
				UJsonFieldData* pageData = Create(contextObject);
				pageData->SetUObject(PropertyName, Value);
				JsonObject->SetObjectField(PropertyName, pageData->Data->GetObjectField(PropertyName));
			}
		}
		else {
			WriteProperty(JsonObject.Get(), Property, Container);
		}*/

	//}

	
}

/**
* Adds the supplied vector to the post data, under the given key
*
* @param	key					Key
* @param	value				Vector
*
* @return	The object itself
*/
UJsonFieldData * UJsonFieldData::SetVector(const FString & key, FVector value)
{
	TArray<TSharedPtr<FJsonValue>> *dataArray = new TArray<TSharedPtr<FJsonValue>>();
	dataArray->Add(MakeShareable(new FJsonValueNumber(value.X)));
	dataArray->Add(MakeShareable(new FJsonValueNumber(value.Y)));
	dataArray->Add(MakeShareable(new FJsonValueNumber(value.Z)));
	Data->SetArrayField(*key, *dataArray);
	return this;
}

/**
* Adds the supplied vector array to the post data, under the given key
*
* @param	key					Key
* @param	arrayData			Vector Array
*
* @return	The object itself
*/
UJsonFieldData * UJsonFieldData::SetVectorArray(const FString & key, const TArray<FVector>& arrayData)
{
	/*	
	TArray<TSharedPtr<FJsonValue>> *Array = new TArray<TSharedPtr<FJsonValue>>();

	// Loop through the array and create new shared FJsonValueObject instances for every FJsonObject
	//Due to the fact that the first object of the json stack is a requirement set serialize an array,the index is used as a key
	for (auto iData : arrayData) 
	{
		TSharedPtr<FJsonValue> DataArray = CreateJsonValueFromVector(iData);
		Array->Add(DataArray);
	}
	
	Data->SetArrayField(*key, *Array);
	*/

	TSharedPtr<FJsonObject> FakeArray = MakeShareable(new FJsonObject());
	for (int i = 0; i < arrayData.Num(); i++)
	{
		FVector value = arrayData[i];
		TArray<TSharedPtr<FJsonValue>> *dataArray = new TArray<TSharedPtr<FJsonValue>>();
		dataArray->Add(MakeShareable(new FJsonValueNumber(value.X)));
		dataArray->Add(MakeShareable(new FJsonValueNumber(value.Y)));
		dataArray->Add(MakeShareable(new FJsonValueNumber(value.Z)));
		FakeArray->SetArrayField(FString::FromInt(i), *dataArray);
	}

	Data->SetObjectField(*key, FakeArray);

	return this;
}

/**
* Adds the supplied Color to the post data, under the given key
*
* @param	key					Key
* @param	value				Color
*
* @return	The object itself
*/
UJsonFieldData * UJsonFieldData::SetColor(const FString & key, FColor value)
{
	TArray<TSharedPtr<FJsonValue>> *dataArray = new TArray<TSharedPtr<FJsonValue>>();
	dataArray->Add(MakeShareable(new FJsonValueNumber(value.R)));
	dataArray->Add(MakeShareable(new FJsonValueNumber(value.G)));
	dataArray->Add(MakeShareable(new FJsonValueNumber(value.B)));
	dataArray->Add(MakeShareable(new FJsonValueNumber(value.A)));
	Data->SetArrayField(*key, *dataArray);
	return this;
}

/**
* Adds the supplied Color Array array to the post data, under the given key
*
* @param	key					Key
* @param	arrayData			Color Array
*
* @return	The object itself
*/
UJsonFieldData * UJsonFieldData::SetColorArray(const FString & key, const TArray<FColor>& arrayData)
{
	TSharedPtr<FJsonObject> FakeArray = MakeShareable(new FJsonObject());
	for (int i = 0; i < arrayData.Num(); i++)
	{
		FColor value = arrayData[i];
		TArray<TSharedPtr<FJsonValue>> *dataArray = new TArray<TSharedPtr<FJsonValue>>();
		dataArray->Add(MakeShareable(new FJsonValueNumber(value.R)));
		dataArray->Add(MakeShareable(new FJsonValueNumber(value.G)));
		dataArray->Add(MakeShareable(new FJsonValueNumber(value.B)));
		dataArray->Add(MakeShareable(new FJsonValueNumber(value.A)));
		FakeArray->SetArrayField(FString::FromInt(i), *dataArray);
	}

	Data->SetObjectField(*key, FakeArray);

	return this;
}

/**
* Adds the supplied Rotator to the post data, under the given key
*
* @param	key					Key
* @param	value				Rotator
*
* @return	The object itself
*/
UJsonFieldData * UJsonFieldData::SetRotator(const FString & key, FRotator value)
{
	TArray<TSharedPtr<FJsonValue>> *dataArray = new TArray<TSharedPtr<FJsonValue>>();
	dataArray->Add(MakeShareable(new FJsonValueNumber(value.Pitch)));
	dataArray->Add(MakeShareable(new FJsonValueNumber(value.Yaw)));
	dataArray->Add(MakeShareable(new FJsonValueNumber(value.Roll)));
	
	Data->SetArrayField(*key, *dataArray);
	return this;
}

/**
* Adds the supplied Transform to the post data, under the given key
*
* @param	key					Key
* @param	value				Transform
*
* @return	The object itself
*/
UJsonFieldData * UJsonFieldData::SetTransform(const FString & key, FTransform value)
{
	TArray<TSharedPtr<FJsonValue>> *transformArray = new TArray<TSharedPtr<FJsonValue>>();
	
	transformArray->Add(MakeShareable(new FJsonValueNumber(value.GetLocation().X)));
	transformArray->Add(MakeShareable(new FJsonValueNumber(value.GetLocation().Y)));
	transformArray->Add(MakeShareable(new FJsonValueNumber(value.GetLocation().Z)));
	
	transformArray->Add(MakeShareable(new FJsonValueNumber(value.GetRotation().X)));
	transformArray->Add(MakeShareable(new FJsonValueNumber(value.GetRotation().Y)));
	transformArray->Add(MakeShareable(new FJsonValueNumber(value.GetRotation().Z)));
	transformArray->Add(MakeShareable(new FJsonValueNumber(value.GetRotation().W)));

	transformArray->Add(MakeShareable(new FJsonValueNumber(value.GetScale3D().X)));
	transformArray->Add(MakeShareable(new FJsonValueNumber(value.GetScale3D().Y)));
	transformArray->Add(MakeShareable(new FJsonValueNumber(value.GetScale3D().Z)));

	Data->SetArrayField(*key, *transformArray);
	return this;
}

/**
* Adds the supplied Class to the post data, under the given key
*
* @param	key					Key
* @param	value				Class
*
* @return	The object itself
*/
UJsonFieldData * UJsonFieldData::SetClass(const FString & key, UClass* value)
{
	if (nullptr != value) {
		FString className = FStringClassReference(value).ToString();
		Data->SetStringField(*key, *className);
	}
	else {
		UE_LOG(LogJson, Error, TEXT("Set an empty Class value"));
	}
	return this;
}

/**
* Adds the supplied Class array to the post data, under the given key
*
* @param	key					Key
* @param	arrayData			Class Array
*
* @return	The object itself
*/
UJsonFieldData * UJsonFieldData::SetClassArray(const FString & key, const TArray<UClass*> arrayData)
{
	TArray<TSharedPtr<FJsonValue>> *classArray = new TArray<TSharedPtr<FJsonValue>>();

	// Loop through the array and create new shared FJsonValueObject instances for every FJsonObject
	for (int32 i = 0; i < arrayData.Num(); i++) {
		FString className = FStringClassReference(arrayData[i]).ToString();
		classArray->Add(MakeShareable(new FJsonValueString(className)));
	}

	Data->SetArrayField(*key, *classArray);
	return this;
}



//
//UJsonFieldData * UJsonFieldData::SetStruct(const FString & key, const UProperty* value)
//{
//	check(value);
//	
//
//
//	return this;
//}

/**
* Adds the supplied object array to the post data, under the given key
*
* @param	key						Key
* @param	objectData				Array of object data
*
* @return	The object itself
*/
UJsonFieldData* UJsonFieldData::SetObjectArray(const FString& key, const TArray<UJsonFieldData*> objectData) {
	TArray<TSharedPtr<FJsonValue>> *dataArray = new TArray<TSharedPtr<FJsonValue>>();

	// Loop through the array and create new shared FJsonValueObject instances for every FJsonObject
	for (int32 i = 0; i < objectData.Num(); i++) {
		dataArray->Add(MakeShareable(new FJsonValueObject(objectData[i]->Data)));
	}

	Data->SetArrayField(*key, *dataArray);
	return this;
}

/**
* Adds the supplied string array to the post data, under the given key
*
* @param	key						Key
* @param	objectData				Array of strings
*
* @return	The object itself
*/
UJsonFieldData* UJsonFieldData::SetStringArray(const FString& key, const TArray<FString> stringData) {
	TArray<TSharedPtr<FJsonValue>> *dataArray = new TArray<TSharedPtr<FJsonValue>>();

	// Loop through the input array and add new shareable FJsonValueString instances to the data array
	for (int32 i = 0; i < stringData.Num(); i++) {
		dataArray->Add(MakeShareable(new FJsonValueString(stringData[i])));
	}

	Data->SetArrayField(*key, *dataArray);
	return this;
}

/**
* Adds the supplied Name to the post data, under the given key
*
* @param	key						Key
* @param	value					Name
*
* @return	The object itself
*/
UJsonFieldData * UJsonFieldData::SetName(const FString & key, const FName & value)
{
	FString fromName = value.ToString();
	Data->SetStringField(*key, *fromName);
	return this;
}

/**
* Adds the supplied Name array to the post data, under the given key
*
* @param	key						Key
* @param	arrayData				Array of Name
*
* @return	The object itself
*/
UJsonFieldData * UJsonFieldData::SetNameArray(const FString & key, const TArray<FName> arrayData)
{
	TArray<TSharedPtr<FJsonValue>> *dataArray = new TArray<TSharedPtr<FJsonValue>>();

	// Loop through the input array and add new shareable FJsonValueString instances to the data array
	for (int32 i = 0; i < arrayData.Num(); i++) {
		dataArray->Add(MakeShareable(new FJsonValueString(arrayData[i].ToString())));
	}

	Data->SetArrayField(*key, *dataArray);
	return this;
}

/**
* Adds the supplied Byte to the post data, under the given key
*
* @param	key						Key
* @param	value					Byte
*
* @return	The object itself
*/
UJsonFieldData * UJsonFieldData::SetByte(const FString & key, uint8 value)
{
	Data->SetNumberField(*key, value);
	return this;
}

/**
* Adds the supplied Byte array to the post data, under the given key
*
* @param	key						Key
* @param	arrayData				Byte of strings
*
* @return	The object itself
*/
UJsonFieldData * UJsonFieldData::SetByteArray(const FString & key, const TArray<uint8> arrayData)
{
	TArray<TSharedPtr<FJsonValue>> *dataArray = new TArray<TSharedPtr<FJsonValue>>();

	// Loop through the input array and add new shareable FJsonValueString instances to the data array
	for (int32 i = 0; i < arrayData.Num(); i++) {
		dataArray->Add(MakeShareable(new FJsonValueNumber(arrayData[i])));
	}

	Data->SetArrayField(*key, *dataArray);
	return this;
}

/**
* Adds the supplied bool to the post data, under the given key
*
* @param	key						Key
* @param	value					Bool
*
* @return	The object itself
*/
UJsonFieldData * UJsonFieldData::SetBool(const FString & key, bool value)
{
	Data->SetNumberField(*key, (value ? 1 : 0));
	return this;
}

/**
* Adds the supplied bool array to the post data, under the given key
*
* @param	key						Key
* @param	arrayData				Array of bool
*
* @return	The object itself
*/
UJsonFieldData * UJsonFieldData::SetBoolArray(const FString & key, const TArray<bool> arrayData)
{
	TArray<TSharedPtr<FJsonValue>> *dataArray = new TArray<TSharedPtr<FJsonValue>>();

	// Loop through the input array and add new shareable FJsonValueString instances to the data array
	for (int32 i = 0; i < arrayData.Num(); i++) {
		dataArray->Add(MakeShareable(new FJsonValueNumber( (arrayData[i] ? 1 : 0) )));
	}

	Data->SetArrayField(*key, *dataArray);
	return this;
}

/**
* Adds the supplied number (float) to the post data, under the given key
*
* @param	key						Key
* @param	value					Float
*
* @return	The object itself
*/
UJsonFieldData * UJsonFieldData::SetNumber(const FString & key, float value)
{
	Data->SetNumberField(*key, value);
	return this;
}

/**
* Adds the supplied number (float) array to the post data, under the given key
*
* @param	key						Key
* @param	arrayData				Array of float
*
* @return	The object itself
*/
UJsonFieldData * UJsonFieldData::SetNumberArray(const FString & key, const TArray<float> arrayData)
{
	TArray<TSharedPtr<FJsonValue>> *dataArray = new TArray<TSharedPtr<FJsonValue>>();

	// Loop through the input array and add new shareable FJsonValueString instances to the data array
	for (int32 i = 0; i < arrayData.Num(); i++) {
		dataArray->Add(MakeShareable(new FJsonValueNumber(arrayData[i])));
	}

	Data->SetArrayField(*key, *dataArray);
	return this;
}

/**
* Gets the post data object from the post data with the given key
*
* @param	WorldContextObject		Array of strings
* @param	key						Key
*
* @return	The object itself
*/
UJsonFieldData* UJsonFieldData::GetObject(const FString& key) const
{
	UJsonFieldData* fieldObj = NULL;

	// Try to get the object field from the data
	const TSharedPtr<FJsonObject> *outPtr;
	if (!Data->TryGetObjectField(*key, outPtr)) {
		// Throw an error and return NULL when the key could not be found
		UE_LOG(LogJson, Error, TEXT("Entry '%s' not found in the field data!"), *key);
		return NULL;
	}

	// Create a new field data object and assign the data
	fieldObj = UJsonFieldData::Create(contextObject);
	fieldObj->Data = *outPtr;

	// Return the newly created object
	return fieldObj;
}

UClass * UJsonFieldData::GetClass(const FString & key) const
{
	FString classPath;
	
	// If the current post data isn't valid, return an empty string
	if (!Data->TryGetStringField(*key, classPath)) {
		UE_LOG(LogJson, Error, TEXT("Entry '%s' not found in the field data!"), *key);
		return nullptr;
	}
	
	//outClass = FindObjectChecked<UClass>(ANY_PACKAGE, *className);
	//return FStringClassReference(classPath).TryLoadClass<UClass>();
	return StaticLoadClass(UObject::StaticClass(), NULL, *classPath, NULL, LOAD_None, NULL);
}

TArray<UClass*> UJsonFieldData::GetClassArray(const FString & key) const
{
	TArray<UClass*> classArray;

	// Try to get the array field from the post data
	const TArray<TSharedPtr<FJsonValue>> *arrayPtr;
	if (Data->TryGetArrayField(*key, arrayPtr)) {
		// Iterate through the array and use the string value from all the entries
		for (int32 i = 0; i < arrayPtr->Num(); i++) {
			FString classPath = (*arrayPtr)[i]->AsString();
			//UClass* classInstance = FindObjectChecked<UClass>(ANY_PACKAGE, *className);
			//UClass* classInstance = FStringClassReference(classPath).TryLoadClass<UClass>();
			UClass* classInstance = StaticLoadClass(UObject::StaticClass(), NULL, *classPath, NULL, LOAD_None, NULL);
			classArray.Add(classInstance);
		}
	}
	else {
		// Throw an error when the entry could not be found in the field data
		UE_LOG(LogJson, Error, TEXT("Array entry '%s' not found in the field data!"), *key);
	}

	// Return the array, if unsuccessful the array will be empty
	return classArray;
}

/**
* Gets a string array from the post data with the given key
*
* @param	key						Key
*
* @return	The requested array of strings
*/
TArray<FString> UJsonFieldData::GetStringArray(const FString& key) const
{
	TArray<FString> stringArray;

	// Try to get the array field from the post data
	const TArray<TSharedPtr<FJsonValue>> *arrayPtr;
	if (Data->TryGetArrayField(*key, arrayPtr)) {
		// Iterate through the array and use the string value from all the entries
		for (int32 i=0; i < arrayPtr->Num(); i++) {
			stringArray.Add((*arrayPtr)[i]->AsString());
		}
	} else {
		// Throw an error when the entry could not be found in the field data
		UE_LOG(LogJson, Error, TEXT("Array entry '%s' not found in the field data!"), *key); 
	}

	// Return the array, if unsuccessful the array will be empty
	return stringArray;
}

FName UJsonFieldData::GetName(const FString & key) const
{

	FString str;

	// If the current post data isn't valid, return an empty string
	if (!Data->TryGetStringField(*key, str)) {
		UE_LOG(LogJson, Error, TEXT("Entry '%s' not found in the field data!"), *key);
		return "";
	}

	return *str;
}

TArray<FName> UJsonFieldData::GetNameArray(const FString & key) const
{
	TArray<FName> stringArray;

	// Try to get the array field from the post data
	const TArray<TSharedPtr<FJsonValue>> *arrayPtr;
	if (Data->TryGetArrayField(*key, arrayPtr)) {
		// Iterate through the array and use the string value from all the entries
		for (int32 i = 0; i < arrayPtr->Num(); i++) {
			stringArray.Add(FName(*((*arrayPtr)[i]->AsString())));
		}
	}
	else {
		// Throw an error when the entry could not be found in the field data
		UE_LOG(LogJson, Error, TEXT("Array entry '%s' not found in the field data!"), *key);
	}

	// Return the array, if unsuccessful the array will be empty
	return stringArray;
}

uint8 UJsonFieldData::GetByte(const FString & key) const
{
	uint32 outByte;

	// If the current post data isn't valid, return an empty string
	if (!Data->TryGetNumberField(*key, outByte)) {
		UE_LOG(LogJson, Error, TEXT("Entry '%s' not found in the field data!"), *key);
		return 0.0;
	}

	return outByte;
}

TArray<uint8> UJsonFieldData::GetByteArray(const FString & key) const
{
	TArray<uint32> numberArray;

	// Try to get the array field from the post data
	const TArray<TSharedPtr<FJsonValue>> *arrayPtr;
	if (Data->TryGetArrayField(*key, arrayPtr)) {
		// Iterate through the array and use the string value from all the entries
		for (int32 i = 0; i < arrayPtr->Num(); i++) {
			numberArray.Add((*arrayPtr)[i]->AsNumber());
		}
	}
	else {
		// Throw an error when the entry could not be found in the field data
		UE_LOG(LogJson, Error, TEXT("Array entry '%s' not found in the field data!"), *key);
	}

	TArray<uint8> outArray;
	outArray.Reset(numberArray.Num());
	for (uint32 i : numberArray)
	{
		outArray.Add(i);
	}

	return outArray;
}

bool UJsonFieldData::GetBool(const FString & key) const
{
	double outBool;

	// If the current post data isn't valid, return an empty string
	if (!Data->TryGetNumberField(*key, outBool)) {
		UE_LOG(LogJson, Error, TEXT("Entry '%s' not found in the field data!"), *key);
		return false;
	}

	return outBool == 1;
}

TArray<bool> UJsonFieldData::GetBoolArray(const FString & key) const
{
	TArray<bool> boolArray;

	// Try to get the array field from the post data
	const TArray<TSharedPtr<FJsonValue>> *arrayPtr;
	if (Data->TryGetArrayField(*key, arrayPtr)) {
		// Iterate through the array and use the string value from all the entries
		for (int32 i = 0; i < arrayPtr->Num(); i++) {
			boolArray.Add( ((*arrayPtr)[i]->AsNumber() == 1) );
		}
	}
	else {
		// Throw an error when the entry could not be found in the field data
		UE_LOG(LogJson, Error, TEXT("Array entry '%s' not found in the field data!"), *key);
	}

	return boolArray;
}

float UJsonFieldData::GetNumber(const FString & key) const
{
	double outNumber;

	// If the current post data isn't valid, return an empty string
	if (!Data->TryGetNumberField(*key, outNumber)) {
		UE_LOG(LogJson, Error, TEXT("Entry '%s' not found in the field data!"), *key);
		return 0.0;
	}

	return outNumber;
}

TArray<float> UJsonFieldData::GetNumberArray(const FString & key) const
{
	TArray<float> numberArray;

	// Try to get the array field from the post data
	const TArray<TSharedPtr<FJsonValue>> *arrayPtr;
	if (Data->TryGetArrayField(*key, arrayPtr)) {
		// Iterate through the array and use the string value from all the entries
		for (int32 i = 0; i < arrayPtr->Num(); i++) {
			numberArray.Add((*arrayPtr)[i]->AsNumber());
		}
	}
	else {
		// Throw an error when the entry could not be found in the field data
		UE_LOG(LogJson, Error, TEXT("Array entry '%s' not found in the field data!"), *key);
	}

	return numberArray;
}

FVector UJsonFieldData::GetVector(const FString & key) const
{
	FVector outVector;
	// Try to get the array field from the post data
	const TArray<TSharedPtr<FJsonValue>> *arrayPtr;
	if (Data->TryGetArrayField(*key, arrayPtr)) {
		// Iterate through the array and use the string value from all the entries
		if(arrayPtr->Num()==3) {
			 outVector.X = ((*arrayPtr)[0]->AsNumber());
			 outVector.Y = ((*arrayPtr)[1]->AsNumber());
			 outVector.Z = ((*arrayPtr)[2]->AsNumber());
		}
		else {
			UE_LOG(LogJson, Error, TEXT("Entry '%s' is not a Vector!"), *key);
		}
	}
	else {
		// Throw an error when the entry could not be found in the field data
		UE_LOG(LogJson, Error, TEXT("Array entry '%s' not found in the field data!"), *key);
	}

	return outVector;
}

TArray<FVector> UJsonFieldData::GetVectorArray(const FString & key) const
{
	TArray<FVector> OutVectorArray;
	//const TArray<TSharedPtr<FJsonValue>> *arrayPtr;
	//if (Data->TryGetArrayField(*key, arrayPtr)) {
	//	// Iterate through the array and use the string value from all the entries
	//	for (TSharedPtr<FJsonValue> JsonValue : *arrayPtr)
	//	{
	//		OutVectorArray.Add(CreateVectorFromJsonValue(JsonValue));
	//	}
	//}
	const TSharedPtr<FJsonObject> * ArrayObject;
	if (Data->TryGetObjectField(*key, ArrayObject))
	{
		for (auto iKey = (*ArrayObject)->Values.CreateConstIterator(); iKey; ++iKey)
		{
			const TSharedPtr<FJsonValue> JsonValue = iKey.Value();
			const FVector iVector = CreateVectorFromJsonValue(JsonValue);
			OutVectorArray.Add(iVector);
		}
	}
	else {
		// Throw an error when the entry could not be found in the field data
		UE_LOG(LogJson, Error, TEXT("Array entry '%s' not found in the field data!"), *key);
	}

	return OutVectorArray;
}

FColor UJsonFieldData::GetColor(const FString & key) const
{
	FColor outVector;
	// Try to get the array field from the post data
	const TArray<TSharedPtr<FJsonValue>> *arrayPtr;
	if (Data->TryGetArrayField(*key, arrayPtr)) {
		// Iterate through the array and use the string value from all the entries
		if (arrayPtr->Num() == 3) {
			outVector.R = ((*arrayPtr)[0]->AsNumber());
			outVector.G = ((*arrayPtr)[1]->AsNumber());
			outVector.B = ((*arrayPtr)[2]->AsNumber());
			outVector.A = 1.0;
		}
		else if (arrayPtr->Num() == 4) {
			outVector.R = ((*arrayPtr)[0]->AsNumber());
			outVector.G = ((*arrayPtr)[1]->AsNumber());
			outVector.B = ((*arrayPtr)[2]->AsNumber());
			outVector.A = ((*arrayPtr)[3]->AsNumber());
		}
		else {
			UE_LOG(LogJson, Error, TEXT("Entry '%s' is not a Color!"), *key);
		}
	}
	else {
		// Throw an error when the entry could not be found in the field data
		UE_LOG(LogJson, Error, TEXT("Array entry '%s' not found in the field data!"), *key);
	}

	return outVector;
}

TArray<FColor> UJsonFieldData::GetColorArray(const FString & key) const
{
	TArray<FColor> OutColorArray;
	const TSharedPtr<FJsonObject> * ArrayObject;
	if (Data->TryGetObjectField(*key, ArrayObject))
	{
		for (auto iKey = (*ArrayObject)->Values.CreateConstIterator(); iKey; ++iKey)
		{
			const TSharedPtr<FJsonValue> JsonValue = iKey.Value();
			const FColor iColor = CreateColorFromJsonValue(JsonValue);
			OutColorArray.Add(iColor);
		}
	}
	else {
		// Throw an error when the entry could not be found in the field data
		UE_LOG(LogJson, Error, TEXT("Array entry '%s' not found in the field data!"), *key);
	}

	return OutColorArray;
}

FRotator UJsonFieldData::GetRotator(const FString & key) const
{
	FRotator outRotator;
	// Try to get the array field from the post data
	const TArray<TSharedPtr<FJsonValue>> *arrayPtr;
	if (Data->TryGetArrayField(*key, arrayPtr)) 
	{
		// Iterate through the array and use the string value from all the entries
		if (arrayPtr->Num() == 3) 
		{
			outRotator = FRotator(
				((*arrayPtr)[0]->AsNumber()),
				((*arrayPtr)[1]->AsNumber()),
				((*arrayPtr)[2]->AsNumber())
			);

		}
		else 
		{
			UE_LOG(LogJson, Error, TEXT("Entry '%s' is not a Rotator!"), *key);
		}
	}
	else
	{
		// Throw an error when the entry could not be found in the field data
		UE_LOG(LogJson, Error, TEXT("Array entry '%s' not found in the field data!"), *key);
	}
	return outRotator;
}

FTransform UJsonFieldData::GetTransform(const FString & key) const
{
	FTransform outTransform;
	
	const TArray<TSharedPtr<FJsonValue>> *arrayPtr;
	if (Data->TryGetArrayField(*key, arrayPtr)) 
	{
		// Iterate through the array and use the string value from all the entries
		if (arrayPtr->Num() == 10)
		{
			FVector loc = FVector(
				((*arrayPtr)[0]->AsNumber()),
				((*arrayPtr)[1]->AsNumber()),
				((*arrayPtr)[2]->AsNumber())
			);
			
			FQuat quat = FQuat(
				((*arrayPtr)[3]->AsNumber()),
				((*arrayPtr)[4]->AsNumber()),
				((*arrayPtr)[5]->AsNumber()),
				((*arrayPtr)[6]->AsNumber())
				);

			FVector scale = FVector(
				((*arrayPtr)[7]->AsNumber()),
				((*arrayPtr)[8]->AsNumber()),
				((*arrayPtr)[9]->AsNumber())
			);
			outTransform = FTransform(quat, loc, scale);

		}
		else {
			UE_LOG(LogJson, Error, TEXT("Entry '%s' is not a Transform!"), *key);
		}
	}
	else {
		// Throw an error when the entry could not be found in the field data
		UE_LOG(LogJson, Error, TEXT("Array entry '%s' not found in the field data!"), *key);
	}


	return outTransform;
}

/**
* Gets an object array from the post data with the given key
*
* @param	key						Key
*
* @return	The requested post data objects
*/
TArray<UJsonFieldData*> UJsonFieldData::GetObjectArray(const FString& key) const
{
	TArray<UJsonFieldData*> objectArray;

	// Try to fetch and assign the array to the array pointer
	const TArray<TSharedPtr<FJsonValue>> *arrayPtr;
	if (Data->TryGetArrayField(*key, arrayPtr)) {
		// Iterate through the input array and create new post data objects for every entry and add them to the objectArray
		for (int32 i = 0; i < arrayPtr->Num(); i++) {
			UJsonFieldData* pageData = Create(contextObject);
			pageData->Data = (*arrayPtr)[i]->AsObject();
			objectArray.Add(pageData);
		}
	}
	else {
		// Throw an error, since the value with the supplied key could not be found
		UE_LOG(LogJson, Error, TEXT("Array entry '%s' not found in the field data!"), *key);
	}

	// Return the array, will be empty if unsuccessful
	return objectArray;
}

/**
* Gets the keys from the supplied object
*
* @param	key						Key
*
* @return	Array of keys
*/
TArray<FString> UJsonFieldData::GetObjectKeys() const 
{
	TArray<FString> stringArray;

	for (auto currJsonValue = Data->Values.CreateConstIterator(); currJsonValue; ++currJsonValue) {
		stringArray.Add((*currJsonValue).Key);
	}

	// Return the array, will be empty if unsuccessful
	return stringArray;
}

/**
* Tries to get a string from the field data by key, returns the string when successful
*
* @param	key			Key
*
* @return	The requested string, empty if failed
*/
FString UJsonFieldData::GetString(const FString& key) const {
	FString outString;

	// If the current post data isn't valid, return an empty string
	if (!Data->TryGetStringField(*key, outString)) {
		UE_LOG(LogJson, Error, TEXT("Entry '%s' not found in the field data!"), *key);
		return "";
	}

	return outString;
}



/**
* Creates new data from the 
*
* @param	key			Key
*
* @return	The requested string, empty if failed
*/
UJsonFieldData* UJsonFieldData::FromString(const FString& dataString) {
	TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(dataString);

	// Deserialize the JSON data
	bool isDeserialized = FJsonSerializer::Deserialize(JsonReader, Data);

	if (!isDeserialized || !Data.IsValid()) {
		UE_LOG(LogJson, Error, TEXT("JSON data is invalid! Input:\n'%s'"), *dataString);
	}

	return this;
}

UJsonFieldData * UJsonFieldData::FromCompressed(const TArray<uint8>& CompressedData,bool& bIsValid)
{
	TArray<TCHAR> UncompressedData;
	
	int32 UncompressedSize;
	FMemory::Memcpy(&UncompressedSize, &CompressedData[0], sizeof(UncompressedSize));
	UncompressedData.SetNum(UncompressedSize);
	bIsValid = FCompression::UncompressMemory(COMPRESS_ZLIB, UncompressedData.GetData(), UncompressedSize, CompressedData.GetData() + sizeof(UncompressedSize), CompressedData.Num() - sizeof(UncompressedSize));

	if (bIsValid) {
		FString StringData = FString(UncompressedSize / sizeof(TCHAR), UncompressedData.GetData());

		FromString(StringData);
	}
	

	return this;
}


TSharedPtr<FJsonObject> UJsonFieldData::CreateJsonValueFromUObjectProperty(const UObjectProperty* InObjectProperty,const void* InObjectData)
{
	check(InObjectProperty);

	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());
	const UObject* Value = InObjectProperty->GetObjectPropertyValue(InObjectData);
	if (!Value) return NULL;

	UClass* ObjectClass = Value->GetClass();
	for (UProperty* Property = ObjectClass->PropertyLink; Property; Property = Property->PropertyLinkNext)
	{
		if (!Property->HasAllPropertyFlags(
			EPropertyFlags::CPF_SaveGame
		)) continue;

		for (int32 ArrayIndex = 0; ArrayIndex < Property->ArrayDim; ArrayIndex++)
		{
			// This grabs the pointer to where the property value is stored
			const void* ValuePtr = Property->ContainerPtrToValuePtr<const void>(Value, ArrayIndex);

			// Parse this property
			WriteProperty(JsonObject.Get(), Property->GetFName().ToString(), Property, ValuePtr);
		}
	}

	//FString className = FStringClassReference(ObjectClass).ToString();
	//JsonObject->SetStringField("__internal__ContainerClass", *className);

	return JsonObject;
}
TSharedPtr<FJsonObject> UJsonFieldData::CreateJsonValueFromUObject(const UObject * InObject)
{
	check(InObject);
	UClass* ObjectClass = InObject->GetClass();
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());

	for (UProperty* Property = ObjectClass->PropertyLink; Property; Property = Property->PropertyLinkNext)
	{
		if (!Property->HasAllPropertyFlags(
			EPropertyFlags::CPF_SaveGame
		)) continue;

		for (int32 ArrayIndex = 0; ArrayIndex < Property->ArrayDim; ArrayIndex++)
		{
			// This grabs the pointer to where the property value is stored
			const void* ValuePtr = Property->ContainerPtrToValuePtr<const void>(InObject, ArrayIndex);

			WriteProperty(JsonObject.Get(), Property->GetFName().ToString(), Property, ValuePtr);
		}
	}

	//FString className = FStringClassReference(ObjectClass).ToString();
	//JsonObject->SetStringField("__internal__ContainerClass", *className);

	return JsonObject;
}

TSharedPtr<FJsonObject> UJsonFieldData::CreateJsonValueFromStruct(const UStructProperty* StructProperty, const void* StructPtr) {

	//check(InStruct);
	TSharedPtr<FJsonObject> JsonStruct = MakeShareable(new FJsonObject());

	// Walk the structs' properties
	UScriptStruct* Struct = StructProperty->Struct;
	for (TFieldIterator<UProperty> It(Struct); It; ++It)
	{
		UProperty* Property = *It;

		// This is the variable name if you need it
		FString VariableName = Property->GetName();

		// Never assume ArrayDim is always 1
		for (int32 ArrayIndex = 0; ArrayIndex < Property->ArrayDim; ArrayIndex++)
		{
			// This grabs the pointer to where the property value is stored
			const void* ValuePtr = Property->ContainerPtrToValuePtr<const void>(StructPtr, ArrayIndex);

			// Parse this property
			//ParseProperty(Property, ValuePtr);
			WriteProperty(JsonStruct.Get(), Property->GetFName().ToString(), Property, ValuePtr);
		}
	}


	return JsonStruct;
}

bool UJsonFieldData::WriteProperty(FJsonObject* JsonWriter, const FString& Identifier, const UProperty* InProperty, const void* InPropertyData)
{
	if (!InProperty) return false;

	//const FString Identifier = InProperty->GetFName().ToString();

	/*if (const UEnumProperty* EnumProp = Cast<const UEnumProperty>(InProperty))
	{
		const FString PropertyValue = DataTableUtils::GetPropertyValueAsString(EnumProp, (uint8*)InRowData, DTExportFlags);
		JsonWriter->WriteValue(Identifier, PropertyValue);
	}
	else */
		
	// Here's how to read integer and float properties
	if (const UNumericProperty *NumericProperty = Cast<const UNumericProperty>(InProperty))
	{
		/*if (NumProp->IsEnum())
		{
			const FString PropertyValue = DataTableUtils::GetPropertyValueAsString(InProperty, (uint8*)InRowData, DTExportFlags);
			JsonWriter->WriteValue(Identifier, PropertyValue);
		}
		else*/
		if (NumericProperty->IsFloatingPoint())
		{
			//auto Data = NumericProperty->ContainerPtrToValuePtr<float>(InPropertyData);
			double FloatValue = NumericProperty->GetFloatingPointPropertyValue(InPropertyData);
		//	UE_LOG(LogJson, Log, TEXT("Float Property %s Value %s"), *Identifier, *FString::SanitizeFloat(FloatValue));

			JsonWriter->SetNumberField(Identifier, FloatValue);
		}
		else if (NumericProperty->IsInteger())
		{
			//auto Data = NumericProperty->ContainerPtrToValuePtr<int>(InPropertyData);
			int64 IntValue = NumericProperty->GetSignedIntPropertyValue(InPropertyData);
			JsonWriter->SetNumberField(Identifier, IntValue);
		}
	}
	else if (auto StrProperty = Cast<UStrProperty>(InProperty))
	{
		//auto Value = StrProperty->GetPropertyValue(StrProperty->ContainerPtrToValuePtr<FString>(InPropertyData));
		auto Value = StrProperty->GetPropertyValue(InPropertyData);
		JsonWriter->SetField(Identifier, MakeShareable(new FJsonValueString(Value)));
	}
	else if (const UClassProperty* ClassProp = Cast<const UClassProperty>(InProperty))
	{
		//auto Data = ClassProp->ContainerPtrToValuePtr<UObject>(InPropertyData);
		UObject* PropertyValue = ClassProp->GetPropertyValue(InPropertyData);
		if (PropertyValue)
		{
			FString className = FStringClassReference(PropertyValue->GetClass()).ToString();
			JsonWriter->SetStringField(Identifier, *className);
		}
	}
	else if (const UBoolProperty* BoolProp = Cast<const UBoolProperty>(InProperty))
	{
		//auto Data = BoolProp->ContainerPtrToValuePtr<bool>(InPropertyData);
		const int PropertyValue = BoolProp->GetPropertyValue(InPropertyData) ? 1 : 0;
		JsonWriter->SetNumberField(Identifier, PropertyValue);
	}
	else if (auto StructProp = Cast<const UStructProperty>(InProperty))
	{
		//const void* Data = StructProp->ContainerPtrToValuePtr<void>(StructProp->Struct, 0);
		//const UStruct* PropertyValue = StructProp->GetPropertyValue(StructProp->ContainerPtrToValuePtr<const UStruct>(InPropertyData));

		//BEST =
		//const UStruct* PropertyValue = StructProp->ContainerPtrToValuePtr<const UStruct>(InPropertyData, 0);
		TSharedPtr<FJsonObject> JsonStruct = CreateJsonValueFromStruct(StructProp, InPropertyData);
		//TSharedPtr<FJsonObject> JsonStruct = CreateJsonValueFromStruct(PropertyValue, Data);

		JsonWriter->SetObjectField(Identifier, JsonStruct);
	}
	else if (auto objectProperty = Cast<UObjectProperty>(InProperty))
	{
		//const void* Data = objectProperty->ContainerPtrToValuePtr<void>(InPropertyData, 0);
		//const UObject* PropertyValue = objectProperty->GetPropertyValue(objectProperty->ContainerPtrToValuePtr<const UObject>(InPropertyData));
		
		//const UObject* PropertyValue = objectProperty->GetObjectPropertyValue(InPropertyData);
		TSharedPtr<FJsonObject> JsonValue = CreateJsonValueFromUObjectProperty(objectProperty, InPropertyData);
		JsonWriter->SetObjectField(Identifier, JsonValue);

		//FString className = FStringClassReference(Value->GetClass()).ToString();
		//JsonWriter->SetStringField("__internal__ContainerClass", *className);
	}
	/*else if (const UArrayProperty* ArrayProp = Cast<const UArrayProperty>(InProperty))
	{
		JsonWriter->WriteArrayStart(Identifier);

		FScriptArrayHelper ArrayHelper(ArrayProp, InPropertyData);
		for (int32 ArrayEntryIndex = 0; ArrayEntryIndex < ArrayHelper.Num(); ++ArrayEntryIndex)
		{
			const uint8* ArrayEntryData = ArrayHelper.GetRawPtr(ArrayEntryIndex);
			WriteContainerEntry(ArrayProp->Inner, ArrayEntryData);
		}

		JsonWriter->WriteArrayEnd();
	}
	else if (const USetProperty* SetProp = Cast<const USetProperty>(InProperty))
	{
		JsonWriter->WriteArrayStart(Identifier);

		FScriptSetHelper SetHelper(SetProp, InPropertyData);
		for (int32 SetSparseIndex = 0; SetSparseIndex < SetHelper.GetMaxIndex(); ++SetSparseIndex)
		{
			if (SetHelper.IsValidIndex(SetSparseIndex))
			{
				const uint8* SetEntryData = SetHelper.GetElementPtr(SetSparseIndex);
				WriteContainerEntry(SetHelper.GetElementProperty(), SetEntryData);
			}
		}

		JsonWriter->WriteArrayEnd();
	}
	else if (const UMapProperty* MapProp = Cast<const UMapProperty>(InProperty))
	{
		JsonWriter->WriteObjectStart(Identifier);

		FScriptMapHelper MapHelper(MapProp, InPropertyData);
		for (int32 MapSparseIndex = 0; MapSparseIndex < MapHelper.GetMaxIndex(); ++MapSparseIndex)
		{
			if (MapHelper.IsValidIndex(MapSparseIndex))
			{
				const uint8* MapKeyData = MapHelper.GetKeyPtr(MapSparseIndex);
				const uint8* MapValueData = MapHelper.GetValuePtr(MapSparseIndex);

				// JSON object keys must always be strings
				const FString KeyValue = DataTableUtils::GetPropertyValueAsStringDirect(MapHelper.GetKeyProperty(), (uint8*)MapKeyData, DTExportFlags);
				WriteContainerEntry(MapHelper.GetValueProperty(), MapValueData, &KeyValue);
			}
		}

		JsonWriter->WriteObjectEnd();
	}*/


	return true;
}


/* Example function for parsing a single property
* @param Property    the property reflection data
* @param ValuePtr    the pointer to the property value
*/
//void UJsonFieldData::ParseProperty(const UProperty* Property, const void* ValuePtr)
//{
//
//	float FloatValue;
//	int32 IntValue;
//	bool BoolValue;
//	FString StringValue;
//	FName NameValue;
//	FText TextValue;
//
//
//	// Here's how to read integer and float properties
//	if (const UNumericProperty *NumericProperty = Cast<const UNumericProperty>(Property))
//	{
//		if (NumericProperty->IsFloatingPoint())
//		{
//			FloatValue = NumericProperty->GetFloatingPointPropertyValue(ValuePtr);
//			UE_LOG(LogJson, Log, TEXT("Float Value in STRUCT %f"), FloatValue);
//		}
//		else if (NumericProperty->IsInteger())
//		{
//			IntValue = NumericProperty->GetSignedIntPropertyValue(ValuePtr);
//		}
//	}
//
//	// How to read booleans
//	if (const UBoolProperty* BoolProperty = Cast<const UBoolProperty>(Property))
//	{
//		BoolValue = BoolProperty->GetPropertyValue(ValuePtr);
//	}
//
//	// Reading names
//	if (const UNameProperty* NameProperty = Cast<const UNameProperty>(Property))
//	{
//		NameValue = NameProperty->GetPropertyValue(ValuePtr);
//	}
//
//	// Reading strings
//	if (const UStrProperty* StringProperty = Cast<const UStrProperty>(Property))
//	{
//		StringValue = StringProperty->GetPropertyValue(ValuePtr);
//	}
//
//	// Reading texts
//	if (const UTextProperty* TextProperty = Cast<const UTextProperty>(Property))
//	{
//		TextValue = TextProperty->GetPropertyValue(ValuePtr);
//	}
//
//	// Reading an array
//	if (const UArrayProperty* ArrayProperty = Cast<const UArrayProperty>(Property))
//	{
//		// We need the helper to get to the items of the array            
//		FScriptArrayHelper Helper(ArrayProperty, ValuePtr);
//		for (int32 i = 0, n = Helper.Num(); i < n; ++i)
//		{
//			ParseProperty(ArrayProperty->Inner, Helper.GetRawPtr(i));
//		}
//	}
//
//	// Reading a nested struct
//	if (const UStructProperty* StructProperty = Cast<const UStructProperty>(Property))
//	{
//		CreateJsonValueFromStruct(StructProperty, ValuePtr);
//	}
//}
/*
* Example function for iterating through all properties of a struct
* @param StructProperty    The struct property reflection data
* @param StructPtr        The pointer to the struct value
*/
//void UJsonFieldData::IterateThroughStructProperty(UStructProperty* StructProperty, void* StructPtr)
//{
//	// Walk the structs' properties
//	UScriptStruct* Struct = StructProperty->Struct;
//	for (TFieldIterator<UProperty> It(Struct); It; ++It)
//	{
//		UProperty* Property = *It;
//
//		// This is the variable name if you need it
//		FString VariableName = Property->GetName();
//
//		// Never assume ArrayDim is always 1
//		for (int32 ArrayIndex = 0; ArrayIndex < Property->ArrayDim; ArrayIndex++)
//		{
//			// This grabs the pointer to where the property value is stored
//			void* ValuePtr = Property->ContainerPtrToValuePtr<void>(StructPtr, ArrayIndex);
//
//			// Parse this property
//			ParseProperty(Property, ValuePtr);
//		}
//	}
//}

