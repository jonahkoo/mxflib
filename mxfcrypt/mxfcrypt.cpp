/*! \file	mxfcrypt.cpp
 *	\brief	MXF en/decrypt utility for MXFLib
 *
 *	\version $Id: mxfcrypt.cpp,v 1.5 2005/02/05 13:11:59 matt-beard Exp $
 *
 */
/*
 *  Copyright (c) 2004, Matt Beard
 *
 *	This software is provided 'as-is', without any express or implied warranty.
 *	In no event will the authors be held liable for any damages arising from
 *	the use of this software.
 *
 *	Permission is granted to anyone to use this software for any purpose,
 *	including commercial applications, and to alter it and redistribute it
 *	freely, subject to the following restrictions:
 *
 *	  1. The origin of this software must not be misrepresented; you must
 *	     not claim that you wrote the original software. If you use this
 *	     software in a product, an acknowledgment in the product
 *	     documentation would be appreciated but is not required.
 *	
 *	  2. Altered source versions must be plainly marked as such, and must
 *	     not be misrepresented as being the original software.
 *	
 *	  3. This notice may not be removed or altered from any source
 *	     distribution.
 */

#include <mxflib/mxflib.h>

using namespace mxflib;

#include <stdio.h>
#include <stdlib.h>


// Include the AS-DCP crypto header
#include "crypto_asdcp.h"


using namespace std;


// Product GUID and version text for this release
Uint8 ProductGUID_Data[16] = { 0x84, 0x62, 0x40, 0xf1, 0x47, 0xed, 0xde, 0x40, 0x86, 0xdc, 0xe0, 0x99, 0xda, 0x7f, 0xd0, 0x53 };
string CompanyName = "freeMXF.org";
string ProductName = "mxfcrypt file de/encrypt utility";
string ProductVersion = "Based on " + LibraryVersion();

//! Plaintext offset to use when encrypting
int PlaintextOffset = 0;

//! Name of keyfile or directoy to search for keyfiles with autogenerated names
std::string KeyFileName;


//! Process a set of header metadata
bool ProcessMetadata(bool DecryptMode, MetadataPtr HMeta, BodyReaderPtr BodyParser, GCWriterPtr Writer, bool LoadInfo = false);

//! Process the metadata for a given package on an encryption pass
bool ProcessPackageForEncrypt(BodyReaderPtr BodyParser, GCWriterPtr Writer, Uint32 BodySID, PackagePtr ThisPackage, bool LoadInfo = false);

//! Process the metadata for a given package on a decryption pass
bool ProcessPackageForDecrypt(BodyReaderPtr BodyParser, GCWriterPtr Writer, Uint32 BodySID, PackagePtr ThisPackage, bool LoadInfo = false);



//! MXFLib debug flag
bool DebugMode = false;

//! Flag set when we are updating the header in the output file to be closed if it is open in the source file (default)
bool ClosingHeader = true;

//! Flag for decrypt rather than encrypt
bool DecryptMode = false;

#include <time.h>

int main(int argc, char *argv[])
{
	printf("MXF en/decrypt utility\n");

	int num_options = 0;
	for(int i=1; i<argc; i++)
	{
		if(argv[i][0] == '-')
		{
			num_options++;
			if((argv[i][1] == 'v') || (argv[i][1] == 'V'))
				DebugMode = true;
			else if((argv[i][1] == 'd') || (argv[i][1] == 'D'))
				DecryptMode = true;
			else if((argv[i][1] == 'f') || (argv[i][1] == 'F'))
				ForceKeyMode = true;
			else if((argv[i][1] == 'h') || (argv[i][1] == 'H'))
				Hashing = true;
			else if((argv[i][1] == 'k') || (argv[i][1] == 'K'))
			{
				if((argv[i][2] != '=') && (argv[i][2] != ':'))
				{
					error("-k option syntax = -k=<key-file or directory>\n");
					return 1;
				}
				KeyFileName = std::string(&argv[i][3]);
			}
			else if((argv[i][1] == 'p') || (argv[i][1] == 'P'))
			{
				if((argv[i][2] != '=') && (argv[i][2] != ':'))
				{
					error("-p option syntax = -p=<plaintextbytes>\n");
					return 1;
				}
				PlaintextOffset = atoi(&argv[i][3]);
				printf("\nPlaintext Offset = %d\n", PlaintextOffset);
			}
		}
	}

	LoadTypes("types.xml");
	MDOType::LoadDict("xmldict.xml");
	MDOType::LoadDict("DMS_Crypto.xml");

//############# TEST
/*
	DataChunk DecKey(16, (Uint8*)"This is the decode key I'm using");
	char *TestData = "Now is the time for all good men to come to the aid of thier hashing system to see if it works exactly as expected first time, second time, and the third and final time indeed as well!!";
	HashPtr Hasher1 = new HashHMACSHA1();
	Hasher1->SetKey(DecKey);
	Hasher1->HashData(strlen(TestData), (Uint8*)TestData);

	DataChunkPtr Hash1 = Hasher1->GetHash();

	printf("Hash 1 is:");
	for(i=0; i<20; i++)
	{
	  printf(" %02x", Hash1->Data[i]);
	}
	printf("\n");


	Uint8 Key2[] = { 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b };

	HashPtr Hasher2 = new HashHMACSHA1();
	Hasher2->SetKey(16, Key2);

	Hasher2->HashData(8, (Uint8*)"Hi There");

	DataChunkPtr Hash2 = Hasher2->GetHash();

	printf("Hash 2 is:");
	for(i=0; i<20; i++)
	{
	  printf(" %02x", Hash2->Data[i]);
	}
	printf("\n");


	char Key2b[] = "our little secret";

	HashPtr Hasher2b = new HashHMACSHA1();
	Hasher2b->SetKey(17, (Uint8*)Key2b);

	Hasher2b->HashData(11, (Uint8*)"hello world");

	DataChunkPtr Hash2b = Hasher2b->GetHash();

	printf("Hash 2b is:");
	for(i=0; i<20; i++)
	{
	  printf(" %02x", Hash2b->Data[i]);
	}
	printf("\n");
	// Should be a7ed9d62819b9788e22171d9108a00c370104526


	HashPtr Hasher3 = new HashHMACSHA1();
	Hasher3->SetKey(DecKey);

	Hasher3->HashData(16, (Uint8*)TestData);
	Hasher3->HashData(16, (Uint8*)&TestData[16]);
	Hasher3->HashData(16, (Uint8*)&TestData[32]);
	Hasher3->HashData(16, (Uint8*)&TestData[48]);
	Hasher3->HashData(strlen(&TestData[64]), (Uint8*)&TestData[64]);

	DataChunkPtr Hash3 = Hasher3->GetHash();

	printf("Hash 3 is:");
	for(i=0; i<20; i++)
	{
	  printf(" %02x", Hash3->Data[i]);
	}
	printf("\n");


*/	
//############# TEST
	
	
	if (argc - num_options < 3)
	{
		printf( "\nUsage:  %s [-d] [-h] [-v] [-k=keyfile] [-p=offset] <in-filename> <out-filename>\n\n", argv[0] );

		return 1;
	}


	MXFFilePtr InFile = new MXFFile;
	if(!InFile->Open(argv[num_options+1], true))
	{
		error("Can't open input file\n");
		return 1;
	}

	// Open the output file
	MXFFilePtr OutFile = new MXFFile;
	if(!OutFile->OpenNew(argv[num_options+2]))
	{
		error("Can't open output file\n");
		return 1;
	}

	// Read the header partition pack
	PartitionPtr MasterPartition = InFile->ReadMasterPartition();

	if(!MasterPartition)
	{
		InFile->Seek(0);
		MasterPartition = InFile->ReadPartition();

		if(!MasterPartition)
		{
			error("Could not read the Header!\n");
			return 1;
		}

		warning("Could not locate a closed partition containing header metadata - attempting to process using open header\n");
	}

	// Read the metadata from the header
	Length Bytes = MasterPartition->ReadMetadata();

	MetadataPtr HMeta = MasterPartition->ParseMetadata();

	if(!HMeta)
	{
		error("Could not load the Header Metadata!\n");
		return 1;
	}

	// Set up a body readyer for the source file
	BodyReaderPtr BodyParser = new BodyReader(InFile);

	// And a writer for the destination file
	// Note that we use a GCWriter rather than a BodyWriter as this allows us to match the
	// layout of the original file body without complications
	GCWriterPtr Writer = new GCWriter(OutFile);

	// Update the header metadata as required - quit if that process failed
	if(!ProcessMetadata(DecryptMode, HMeta, BodyParser, Writer, true)) return 1;

	/* Write the header partition with updated closed metadata if required */

	if(ClosingHeader)
	{
		// If the master partition is not from the header then change it to be a header
		if(MasterPartition->GetUint64("ThisPartition") > 0)
		{
			if(MasterPartition->IsClosed())
			{
				if(MasterPartition->IsComplete()) 
					MasterPartition->ChangeType("ClosedCompleteHeader");
				else
					MasterPartition->ChangeType("ClosedHeader");
			}
			else
			{
				if(MasterPartition->IsComplete()) 
					MasterPartition->ChangeType("OpenCompleteHeader");
				else
					MasterPartition->ChangeType("OpenHeader");
			}

			// Read the old header partition
			InFile->Seek(0);
			PartitionPtr OldHeader = InFile->ReadPartition();

			// Set the header to have the same KAG and BodySID as before
			MasterPartition->SetKAG(OldHeader->GetUint("KAGSize"));
			MasterPartition->SetUint("BodySID", OldHeader->GetUint("BodySID"));
			MasterPartition->SetUint64("FooterPartition", 0);
		}

		// We don't yet know where the footer is...
		MasterPartition->SetUint64("FooterPosition", 0);

		// Write the new header
		OutFile->WritePartition(MasterPartition);
	}

	// Process the file...

	// Start at the beginning of the file
	InFile->Seek(0);

	// Loop until all is done...
	for(;;)
	{
		if(!BodyParser->IsAtPartition())
		{
			BodyParser->ReSync();
		}

		// Move the main file pointer to the current body partition pack
		Position CurrentPos = BodyParser->Tell();
		InFile->Seek(CurrentPos);

		// Read the partition pack
		PartitionPtr CurrentPartition = InFile->ReadPartition();
		if(!CurrentPartition) break;

		/* Update the partition pack ?? */
		
		// Work out if we should do anything with this partition at all
		bool UpdatePartition = true;
		
		// Don't update the header if we have just written an updated closed version 
		if((CurrentPos==0) && (ClosingHeader)) UpdatePartition = false;
		else
			// Don't update the footer - we will write that later
			if(CurrentPartition->IsA("CompleteFooter") || CurrentPartition->IsA("Footer")) UpdatePartition = false;

		if(UpdatePartition)
		{
			// TODO: We should probably insert updated metadata here if the input file has it
			CurrentPartition->SetUint64("FooterPartition", 0);
			OutFile->WritePartition(CurrentPartition);
		}

		// Find out what BodySID
		// DARGONS: ?? What are we doing with this?
		Uint32 BodySID = CurrentPartition->GetUint("BodySID");

		// Ensure we match the KAG
		Writer->SetKAG(CurrentPartition->GetUint("KAGSize"));

		// Parse the file until next partition or an error
		if (!BodyParser->ReadFromFile()) break;
	}

	// Write the footer partition
	
	if(MasterPartition->IsComplete()) 
		MasterPartition->ChangeType("CompleteFooter");
	else
		MasterPartition->ChangeType("Footer");

	// Ensure we maintain the same KAG as the previous footer
	MasterPartition->SetKAG(Writer->GetKAG());

	OutFile->WritePartition(MasterPartition);

	// Add a RIP
	OutFile->WriteRIP();

	InFile->Close();

	OutFile->Close();

	printf("Done\n");

	return 0;


	//############ WE NEED TO HAVE ONE WRITER PER BODY-SID!!
	//############ WE NEED TO HAVE ONE WRITER PER BODY-SID!!
	//############ WE NEED TO HAVE ONE WRITER PER BODY-SID!!
	//############ WE NEED TO HAVE ONE WRITER PER BODY-SID!!
	//############ WE NEED TO HAVE ONE WRITER PER BODY-SID!!
	//############ WE NEED TO HAVE ONE WRITER PER BODY-SID!!
	//############ WE NEED TO HAVE ONE WRITER PER BODY-SID!!
	//############ WE NEED TO HAVE ONE WRITER PER BODY-SID!!
}


//! Process a set of header metadata
/*!	If encrypting a crypto context is added in each internal file package, otherwise crypto tracks are removed
 *	/ret true if all OK, else false
 */
bool ProcessMetadata(bool DecryptMode, MetadataPtr HMeta, BodyReaderPtr BodyParser, GCWriterPtr Writer, bool LoadInfo /*=false*/)
{
	// Locate the Content Storage set
	MDObjectPtr ContentStorage = HMeta["ContentStorage"];
	if(ContentStorage) ContentStorage = ContentStorage->GetLink();

	if(!ContentStorage)
	{
		error("Header Metadata does not contain a ContentStorage set!\n");
		return false;
	}

	// And locate the Essence Container Data list in the Content Storage set
	MDObjectPtr EssenceContainerData = ContentStorage["EssenceContainerData"];

	if(!EssenceContainerData)
	{
		error("ContentStorage set does not contain an EssenceContainerData property!\n");
		return false;
	}

	// A map of PackageIDs of all contained essence, indexed by BodySID
	typedef std::map<Uint32, DataChunkPtr> DataChunkMap;
	DataChunkMap FilePackageMap;

	// Scan the essence containers
	MDObject::iterator it = EssenceContainerData->begin();
	while(it != EssenceContainerData->end())
	{
		MDObjectPtr ECDSet = (*it).second->GetLink();

		if(ECDSet)
		{
			// Add the package ID to the BodySID map
			Uint32 BodySID = ECDSet->GetUint("BodySID");
			MDObjectPtr PackageID = ECDSet["LinkedPackageUID"];
			if(PackageID) 
			{
				DataChunkPtr PackageIDData = PackageID->PutData();
				FilePackageMap.insert(DataChunkMap::value_type(BodySID, PackageIDData ));
			}
		}

		it++;
	}


	/* Add cryptographic context sets (one per internal file package) */

	// Count of number of packages being en/decrypted
	int CryptoCount = 0;

	PackageList::iterator Package_it = HMeta->Packages.begin();
	while(Package_it != HMeta->Packages.end())
	{
		// Locate the package ID
		MDObjectPtr ThisIDObject = (*Package_it)["PackageUID"];
		if(ThisIDObject)
		{
			// Build a datachunk of the UMID to compare
			DataChunkPtr PackageID = ThisIDObject->PutData();

			// Look for a matching BodySID (to see if this is an internal file package)
			DataChunkMap::iterator PackageMap_it = FilePackageMap.begin();
			while(PackageMap_it != FilePackageMap.end())
			{
				// If the package IDs match we are encrypting this package
				if(*((*PackageMap_it).second) == *PackageID)
				{
					bool Result;
					if(DecryptMode) Result = ProcessPackageForDecrypt(BodyParser, Writer, (*PackageMap_it).first, (*Package_it), LoadInfo);
					else Result = ProcessPackageForEncrypt(BodyParser, Writer, (*PackageMap_it).first, (*Package_it), LoadInfo);

					// Exit on error (ignore if we are forcing a key)
					if((!Result) && (!ForceKeyMode)) return false;

					CryptoCount++;
				}

				PackageMap_it++;
			}
		}

		Package_it++;
	}
	

	// Are we actually en/decrypting anything?
	if(CryptoCount == 0)
	{
		if(DecryptMode)
			error("Didn't find a file package for any encrypted essence to decrypt!\n");
		else
			error("Didn't find a file package for any essence to encrypt!\n");

		return false;
	}


	// Update DMSchemes as required
	MDOTypePtr FrameworkLabel = MDOType::Find("CryptographicFrameworkLabel");
	if(!FrameworkLabel)
	{
		error("Couldn't find CryptographicFrameworkLabel in the dictionary - are the correct files loaded?\n");
	}
	else
	{
		DataChunkPtr FrameworkUL = new DataChunk(16, FrameworkLabel->GetUL()->GetValue());

		// Locate the Content Storage set
		MDObjectPtr DMSchemes = HMeta["DMSchemes"];

		if(!DMSchemes)
		{
			error("Header Metadata does not contain a DMSchemes!\n");
			
			// Try and add one
			DMSchemes = HMeta->AddChild("DMSchemes");
			
			// If that fails give up!
			if(!DMSchemes) return false;
		}

		if(DecryptMode)
		{
			bool Found = false;
			MDObject::iterator it = DMSchemes->begin();
			while(it != DMSchemes->end())
			{
				DataChunkPtr ThisLabel = (*it).second->PutData();
				ULPtr ThisUL = new UL(ThisLabel->Data);
				if(*ThisUL == *FrameworkLabel->GetUL())
				{
					DMSchemes->RemoveChild((*it).second);
					Found = true;
					break;
				}
				it++;
			}
			if(!Found)
			{
				error("Source file does not have a CryptographicFrameworkLabel in the DMSchemes list - is it really an AS-DCP encrypted file?\n");
			}
		}
		else
		{
			bool Found = false;
			MDObject::iterator it = DMSchemes->begin();
			while(it != DMSchemes->end())
			{
				DataChunkPtr ThisLabel = (*it).second->PutData();
				ULPtr ThisUL = new UL(ThisLabel->Data);
				if(ThisUL == FrameworkLabel->GetUL())
				{
					Found = true;
					break;
				}
				it++;
			}
			if(Found)
			{
				error("Source file already contains a CryptographicFrameworkLabel in the DMSchemes list - is it already encrypted?\n");
			}
			else
			{
				// Add the crypto scheme
				MDObjectPtr Ptr = DMSchemes->AddChild("DMScheme", false);
				if(Ptr) Ptr->ReadValue(FrameworkLabel->GetUL()->GetValue(), 16);
			}
		}
	}

	// Build an Ident set describing us and link into the metadata
	MDObjectPtr Ident = new MDObject("Identification");
	Ident->SetString("CompanyName", CompanyName);
	Ident->SetString("ProductName", ProductName);
	Ident->SetString("VersionString", ProductVersion);
	Ident->SetString("ToolkitVersion", LibraryProductVersion());
	UUIDPtr ProductUID = new mxflib::UUID(ProductGUID_Data);

	// DRAGONS: -- Need to set a proper GUID per released version
	//             Non-released versions currently use a random GUID
	//			   as they are not a stable version...
	Ident->SetValue("ProductUID", DataChunk(16,ProductUID->GetValue()));

	// Link the new Ident set with all new metadata
	// Note that this is done even for OP-Atom as the 'dummy' header written first
	// could have been read by another device. This flags that items have changed.
	HMeta->UpdateGenerations(Ident);

	return true;
}
	
//! Process the metadata for a given package on an encryption pass
/*! /ret true if all OK, else false
 */
bool ProcessPackageForEncrypt(BodyReaderPtr BodyParser, GCWriterPtr Writer, Uint32 BodySID, PackagePtr ThisPackage, bool LoadInfo /*=false*/)
{
	MDObjectPtr Descriptor = ThisPackage["Descriptor"];
	if(Descriptor) Descriptor = Descriptor->GetLink();

	if(!Descriptor)
	{
		error("Source file contains a File Package without a File Descriptor\n");
		return false;
	}
	
	MDObjectPtr ContainerUL = Descriptor["EssenceContainer"];
	if(!ContainerUL)
	{
		error("Source file contains a File Descriptor without an EssenceContainer label\n");
		return false;
	}

	// Record the original essence UL
	DataChunkPtr EssenceUL = ContainerUL->PutData();

	// Change the essence UL in the descriptor to claim to be encrypted
	const Uint8 EncryptedEssenceUL[] = { 0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x07, 0x0d, 0x01, 0x03, 0x01, 0x02, 0x0b, 0x01, 0x00 };
	ContainerUL->ReadValue(EncryptedEssenceUL, 16);

	// Add a crypto track
	TrackPtr CryptoDMTrack = ThisPackage->AddDMTrack("Cryptographic DM Track");

	// Add metadata to the track
	DMSegmentPtr CryptoDMSegment = CryptoDMTrack->AddDMSegment();

	// Build the cryptographic framework
	MDObjectPtr CryptoFramework = new MDObject("CryptographicFramework");

	// This is the first chance to sanity check the crypto dictionary
	if(!CryptoFramework)
	{
		// DRAGONS: These error messages should be folded by the compiler as they are identical
		error("Failed to build cryptographic metadata - has the correct dictionary been loaded?\n");
		return false;
	}

	// Link the framework to this track
	CryptoDMSegment->MakeLink(CryptoFramework);

	// Build the cryptographic context
	MDObjectPtr CryptoContext = new MDObject("CryptographicContext");
	if(!CryptoContext)
	{
		// DRAGONS: These error messages should be folded by the compiler as they are identical
		error("Failed to build cryptographic metadata - has the correct dictionary been loaded?\n");
		return false;
	}

	// Build the context ID link
	MDObjectPtr ContextSR = CryptoFramework->AddChild("ContextSR");
	if(!ContextSR)
	{
		// DRAGONS: These error messages should be folded by the compiler as they are identical
		error("Failed to build cryptographic metadata - has the correct dictionary been loaded?\n");
		return false;
	}

	// Link us to the framework
	ContextSR->MakeLink(CryptoContext);
	
	// Build a new UUID for the Crypto Context ID
	UUIDPtr ContextID = new mxflib::UUID;
	
	// Set the context ID
	MDObjectPtr Ptr = CryptoContext->AddChild("ContextID");
	if(Ptr) Ptr->ReadValue(ContextID->GetValue(), 16);

	// Set the original essence UL
	Ptr = CryptoContext->AddChild("SourceEssenceContainer");
	if(Ptr) Ptr->ReadValue(EssenceUL);

	// Set the encryption algorithm
	const Uint8 CypherLabel[] = { 0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x07, 0x02, 0x09, 0x02, 0x01, 0x01, 0x00, 0x00, 0x00 };
	Ptr = CryptoContext->AddChild("CipherAlgorithm");
	if(Ptr) Ptr->ReadValue(CypherLabel, 16);

	// Specify no MIC
	const Uint8 MICLabel_NULL[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	const Uint8 MICLabel_HMAC_SHA1[] = { 0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x07, 0x02, 0x09, 0x02, 0x02, 0x01, 0x00, 0x00, 0x00 };
	Ptr = CryptoContext->AddChild("MICAlgorithm");
	if(Ptr) 
	{
		if(Hashing)
			Ptr->ReadValue(MICLabel_HMAC_SHA1, 16);
		else
			Ptr->ReadValue(MICLabel_NULL, 16);
	}


	// Use the specified key
	char *NameBuffer = new char[KeyFileName.size() + 1];
	strcpy(NameBuffer, KeyFileName.c_str());
	
	// Scan back for the last directory seperator to find the filename
	char *NamePtr = &NameBuffer[strlen(NameBuffer)];
	while(NamePtr > NameBuffer)
	{
		if((*NamePtr == '/') || (*NamePtr == DIR_SEPARATOR))
		{
			NamePtr++;
			break;
		}
		NamePtr--;
	}

	// DRAGONS: Build in an int array for type-safety
	int KeyBuff[16];
	int Count = sscanf(NamePtr, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
					   &KeyBuff[0], &KeyBuff[1], &KeyBuff[2], &KeyBuff[3], &KeyBuff[4], &KeyBuff[5], &KeyBuff[6], &KeyBuff[7], 
					   &KeyBuff[8], &KeyBuff[9], &KeyBuff[10], &KeyBuff[11], &KeyBuff[12], &KeyBuff[13], &KeyBuff[14], &KeyBuff[15] );

	delete[] NameBuffer;

	if(Count != 16)
	{
		error("Key filename is not in the correct hex format of: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx\n");
		return false;
	}

	// Copy the datat into a byte buffer
	Uint8 KeyBuffU8[16];
	{
		int i;
		for(i=0; i<16; i++) KeyBuffU8[i] = (Uint8)KeyBuff[i];
	}

	Ptr = CryptoContext->AddChild("CryptographicKeyID");
	if(Ptr) Ptr->ReadValue(KeyBuffU8, 16);

	/* Now set up the crypto handlers */

	// If we haven't already set up this BodySID, do it now
	if(LoadInfo && (!BodyParser->GetGCReader(BodySID)))
	{
		DataChunkPtr KeyID = new DataChunk(16, KeyBuffU8);
		Encrypt_GCReadHandler *pHandler = new Encrypt_GCReadHandler(Writer, BodySID, ContextID, KeyID, KeyFileName);
		pHandler->SetPlaintextOffset(PlaintextOffset);
		GCReadHandlerPtr Handler = pHandler;
		GCReadHandlerPtr FillerHandler = new Basic_GCFillerHandler(Writer, BodySID);
		BodyParser->MakeGCReader(BodySID, Handler, FillerHandler);
	}

	return true;
}



//! Process the metadata for a given package on a decryption pass
/*! /ret true if all OK, else false
 */
bool ProcessPackageForDecrypt(BodyReaderPtr BodyParser, GCWriterPtr Writer, Uint32 BodySID, PackagePtr ThisPackage, bool LoadInfo /*=false*/)
{
	// Decryption Key
	DataChunkPtr Key;

	// Search for the crypto context
	TrackList::iterator it = ThisPackage->Tracks.begin();
	while(it!= ThisPackage->Tracks.end())
	{
//printf("Track: %s\n", (*it)->GetString("TrackName").c_str());
		ComponentList::iterator comp_it = (*it)->Components.begin();
		while(comp_it!= (*it)->Components.end())
		{
//printf("  Comp: %s\n", (*comp_it)->FullName().c_str());
			// Found a DM segment?
			if((*comp_it)->IsA("DMSegment"))
			{
				MDObjectPtr Framework = (*comp_it)->Child("DMFramework");
				if(Framework) Framework = Framework->GetLink();

				// Found a Crypto Framework on the segment?
				if(Framework && Framework->IsA("CryptographicFramework"))
				{
					MDObjectPtr Context = Framework->Child("ContextSR");
					if(Context) Context = Context->GetLink();

					if(Context)
					{
						// Read the key ID
						Key = Context["CryptographicKeyID"]->PutData();

						// Remove the crypto track
						ThisPackage->RemoveTrack(*it);

						break;
					}
				}
			}
			comp_it++;
		}

		// Stop looking once we find the key
		if(Key) break;

		it++;
	}

	// Don't validate or set up crypto if not loading data
	if(!LoadInfo) return true;

//## warning("Not checking if this package is actually encrypted or not!!\n");
//## warning("Not checking if this package is actually encrypted or not!!\n");
//## warning("Not checking if this package is actually encrypted or not!!\n");
//## warning("Not checking if this package is actually encrypted or not!!\n");
//## warning("Not checking if this package is actually encrypted or not!!\n");
//## warning("Not checking if this package is actually encrypted or not!!\n");
//## warning("Not checking if this package is actually encrypted or not!!\n");
//## warning("Not checking if this package is actually encrypted or not!!\n");
//## warning("Not checking if this package is actually encrypted or not!!\n");
//## warning("Not checking if this package is actually encrypted or not!!\n");
//## warning("Not checking if this package is actually encrypted or not!!\n");
//## warning("Not checking if this package is actually encrypted or not!!\n");
//## warning("Not checking if this package is actually encrypted or not!!\n");
//## warning("Not checking if this package is actually encrypted or not!!\n");
	
	if(!Key)
	{
		error("Coundn't find CryptographicKeyID in the encrypted file\n");
		if(!ForceKeyMode) return false;
	}

	GCReadHandlerPtr Handler = new Decrypt_GCReadHandler(Writer, BodySID);
	GCReadHandlerPtr FillerHandler = new Basic_GCFillerHandler(Writer, BodySID);
	GCReadHandlerPtr EncHandler = new Decrypt_GCEncryptionHandler(BodySID, Key, KeyFileName);

	Decrypt_GCEncryptionHandler *Test = SmartPtr_Cast(EncHandler, Decrypt_GCEncryptionHandler);
	if(!Test->KeyValid()) return false;

	BodyParser->MakeGCReader(BodySID, Handler, FillerHandler);
	GCReaderPtr Reader = BodyParser->GetGCReader(BodySID);
	if(Reader) Reader->SetEncryptionHandler(EncHandler);

	return true;
}



// Debug and error messages
#include <stdarg.h>

#ifdef MXFLIB_DEBUG
//! Display a general debug message
void mxflib::debug(const char *Fmt, ...)
{
	if(!DebugMode) return;

	va_list args;

	va_start(args, Fmt);
	vprintf(Fmt, args);
	va_end(args);
}
#endif // MXFLIB_DEBUG

//! Display a warning message
void mxflib::warning(const char *Fmt, ...)
{
	va_list args;

	va_start(args, Fmt);
	printf("Warning: ");
	vprintf(Fmt, args);
	va_end(args);
}

//! Display an error message
void mxflib::error(const char *Fmt, ...)
{
	va_list args;

	va_start(args, Fmt);
	printf("ERROR: ");
	vprintf(Fmt, args);
	va_end(args);
}

