/*! \file	esp_wavepcm.h
 *	\brief	Definition of class that handles parsing of uncompressed pcm wave audio files
 *
 *	\version $Id: esp_wavepcm.h,v 1.14 2011/01/10 10:42:09 matt-beard Exp $
 *
 */
/*
 *	Copyright (c) 2003, Matt Beard
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

#ifndef MXFLIB__ESP_WAVEPCM_H
#define MXFLIB__ESP_WAVEPCM_H

#include <math.h>	// For "floor"


namespace mxflib
{
	class WAVE_PCM_EssenceSubParser : public EssenceSubParserBase
	{
	protected:
		UInt32 SampleRate;									//!< The sample rate of this essence
		Rational UseEditRate;								//!< The edit rate to use for wrapping this essence

		Position DataStart;									//!< Start of "data" chunk (value part)
		Length DataSize;									//!< Size of "data" chunk (value part)
		Position CurrentPosition;							//!< Current position in the input file in edit units
		Position BytePosition;								//!< Current position in the input file in bytes
															/*!< A value of 0 means the start of the data chunk,
															 *	 any other value is that position within the whole file.
															 *	 This means that a full rewind can be achieved by setting BytePosition = 0
															 *	 \note Other functions may move the file
															 *         pointer between calls to our functions */

		size_t CachedDataSize;								//!< The size of the next data to be read, or (size_t)-1 if not known
		UInt64 CachedCount;									//!< The number of wrapping units that CachedDataSize relates to

		int SampleSize;										//!< Size of each sample in bytes (includes all channels)
		UInt32 ConstSamples;								//!< Number of samples per edit unit (if constant, else zero)
		int SampleSequenceSize;								//!< Size of SampleSequence if used
		UInt32 *SampleSequence;								//!< Array of counts of samples per edit unit for non integer relationships between edit rate and sample rate
		int SequencePos;									//!< Current position in the sequence (i.e. next entry to use)

		MDObjectParent CurrentDescriptor;					//!< Pointer to the last essence descriptor we built
															/*!< This is used as a quick-and-dirty check that we know how to process this source */

	public:
		//! Class for EssenceSource objects for parsing/sourcing MPEG-VES essence
		class ESP_EssenceSource : public EssenceSubParserBase::ESP_EssenceSource
		{
		protected:
			size_t BytesRemaining;							//!< The number of bytes remaining in a multi-part GetEssenceData, or zero if not part read

			bool PaddingEnabled;							//!< Is padding after the end of the essence stream enabled?
			DataChunkPtr PaddingChunk;						//!< A chunk containing padding bytes (if required) for use in GetPadding()

		public:
			//! Construct and initialise for essence parsing/sourcing
			ESP_EssenceSource(EssenceSubParserPtr TheCaller, FileHandle InFile, UInt32 UseStream, UInt64 Count = 1/*, IndexTablePtr UseIndex = NULL*/)
				: EssenceSubParserBase::ESP_EssenceSource(TheCaller, InFile, UseStream, Count/*, UseIndex*/) 
			{
				BytesRemaining = 0;
				PaddingEnabled = false;
			};

			//! Get the size of the essence data in bytes
			/*! \note There is intentionally no support for an "unknown" response 
			 */
			virtual size_t GetEssenceDataSize(void) 
			{
				WAVE_PCM_EssenceSubParser *pCaller = SmartPtr_Cast(Caller, WAVE_PCM_EssenceSubParser);
				return pCaller->ReadInternal(File, Stream, RequestedCount);
			};

			//! Get the next "installment" of essence data
			/*! \return Pointer to a data chunk holding the next data or a NULL pointer when no more remains
			 *	\note If there is more data to come but it is not currently available the return value will be a pointer to an empty data chunk
			 *	\note If Size = 0 the object will decide the size of the chunk to return
			 *	\note On no account will the returned chunk be larger than MaxSize (if MaxSize > 0)
			 */
			virtual DataChunkPtr GetEssenceData(size_t Size = 0, size_t MaxSize = 0);

			//! Did the last call to GetEssenceData() return the end of a wrapping item
			/*! \return true if the last call to GetEssenceData() returned an entire wrapping unit.
			 *  \return true if the last call to GetEssenceData() returned the last chunk of a wrapping unit.
			 *  \return true if the last call to GetEssenceData() returned the end of a clip-wrapped clip.
			 *  \return false if there is more data pending for the current wrapping unit.
			 *  \return false if the source is to be clip-wrapped and there is more data pending for the clip
			 */
			virtual bool EndOfItem(void) 
			{ 
				// Items end when there is no data remaining from the last read
				return !BytesRemaining;
			}

			//! Get data to write as padding after all real essence data has been processed
			/*! If more than one stream is being wrapped, they may not all end at the same wrapping-unit.
			 *	When this happens each source that has ended will produce NULL is response to GetEssenceData().
			 *	The default action of the caller would be to write zero-length KLVs in each wrapping unit for each source that has ended.
			 *	If a source supplies an overload for this method, the supplied padding data will be written in wrapping units following the end of essence instead of a zero-length KLV
			 *	DRAGONS: Note that as the returned value is a non-smart pointer, ownership of the buffer stays with the EssenceSource object.
			 *			 The recommended method of operation is to have a member DataChunk (or DataChunkPtr) allocated the first time padding is required, and return the address each call.
			 *			 The destructor must then free the DataChunk, or allow the smart DataChunkPtr to do it automatically
			 */
			virtual DataChunk *GetPadding(void);

			//! Get the preferred BER length size for essence KLVs written from this source, 0 for auto
			virtual int GetBERSize(void) 
			{ 
				WAVE_PCM_EssenceSubParser *pCaller = SmartPtr_Cast(Caller, WAVE_PCM_EssenceSubParser);

				if(pCaller->SelectedWrapping->ThisWrapType == WrappingOption::Clip) return 8;
				return 4;
			}

			//! Set a source type or parser specific option
			/*! \return true if the option was successfully set */
			virtual bool SetOption(std::string Option, Int64 Param = 0);
		};

		// Give our essence source class privilaged access
		friend class WAVE_PCM_EssenceSubParser::ESP_EssenceSource;

	public:
		WAVE_PCM_EssenceSubParser()
		{
			SampleRate = 1;
			ConstSamples = 0;
			SampleSequenceSize = 0;
			SampleSequence = NULL;
			SequencePos = 0;
			DataStart = 0;
			DataSize = 0;
			CurrentPosition = 0;
			BytePosition = 0;

			// Use a sensible default if no edit rate is set - not ideal, but better than one sample!
			// It will always be possible to wrap at this rate, but the end of the data may not be a whole edit unit
			UseEditRate.Numerator = 1;
			UseEditRate.Denominator = 1;

			CachedDataSize = static_cast<size_t>(-1);
			CachedCount = 0;
		}

		//! Clean up and free memory
		~WAVE_PCM_EssenceSubParser()
		{
			if(SampleSequence) delete [] SampleSequence;
		}

		//! Build a new parser of this type and return a pointer to it
		virtual EssenceSubParserPtr NewParser(void) const { return new WAVE_PCM_EssenceSubParser; }

		//! Report the extensions of files this sub-parser is likely to handle
		virtual StringList HandledExtensions(void)
		{
			StringList ExtensionList;

			ExtensionList.push_back("WAV");

			return ExtensionList;
		}

		//! Examine the open file and return a list of essence descriptors
		virtual EssenceStreamDescriptorList IdentifyEssence(FileHandle InFile);

		//! Examine the open file and return the wrapping options known by this parser
		virtual WrappingOptionList IdentifyWrappingOptions(FileHandle InFile, EssenceStreamDescriptor &Descriptor);

		//! Set a wrapping option for future Read and Write calls
		/*! \return true if this EditRate is acceptable with this wrapping */
		virtual void Use(UInt32 Stream, WrappingOptionPtr &UseWrapping)
		{
			SelectedWrapping = UseWrapping;

			BytePosition = 0;
		}


		//! Set a non-native edit rate
		/*! Must be called <b>after</b> Use()
		 *	\return true if this rate is acceptable 
		 */
		virtual bool SetEditRate(Rational EditRate)
		{
			// See if we can figure out a sequence for this rate
			bool Ret = CalcWrappingSequence(EditRate);

			// If we can then set the rate
			if(Ret)
			{
				SequencePos = 0;
				UseEditRate = EditRate;
			}

			return Ret;
		}

		//! Get the current edit rate
		virtual Rational GetEditRate(void) { return UseEditRate; }

		//! Get the preferred edit rate (if one is known)
		/*! \return The prefered edit rate or 0/0 if note known
		 */
		virtual Rational GetPreferredEditRate(void);

		//! Get BytesPerEditUnit, if Constant
		virtual UInt32 GetBytesPerEditUnit(UInt32 KAGSize = 1);

		//! Get the current position in SetEditRate() sized edit units
		virtual Position GetCurrentPosition(void) { return CurrentPosition; }

		//! Read a number of wrapping items from the specified stream and return them in a data chunk
		virtual DataChunkPtr Read(FileHandle InFile, UInt32 Stream, UInt64 Count = 1/*, IndexTablePtr Index = NULL*/);

		//! Build an EssenceSource to read a number of wrapping items from the specified stream
		virtual EssenceSourcePtr GetEssenceSource(FileHandle InFile, UInt32 Stream, UInt64 Count = 1/*, IndexTablePtr Index = NULL*/)
		{
			return new ESP_EssenceSource(this, InFile, Stream, Count/*, Index*/);
		};

		//! Write a number of wrapping items from the specified stream to an MXF file
		virtual Length Write(FileHandle InFile, UInt32 Stream, MXFFilePtr OutFile, UInt64 Count = 1/*, IndexTablePtr Index = NULL*/);

		//! Get a unique name for this sub-parser
		/*! The name must be all lower case, and must be unique.
		 *  The recommended name is the part of the filename of the parser header after "esp_" and before the ".h".
		 *  If the parser has no name return "" (however this will prevent named wrapping option selection for this sub-parser)
		 */
		virtual std::string GetParserName(void) const { return "wavepcm"; }


	protected:
		//! Work out wrapping sequence
		bool CalcWrappingSequence(Rational EditRate);

		//! Calculate the current position in SetEditRate() sized edit units from "BytePosition" in bytes
		Position CalcCurrentPosition(void);

		//! Get the number of samples this edit unit
		UInt32 SamplesThisEditUnit(void);

		//! Undo the last step through the wrapping sequence done by SamplesThisEditUnit()
		void PushBackSize(void)
		{
			// If no sequence, skip
			if(ConstSamples) return;

			// If no edit rate has been set, skip
			if((SampleSequenceSize == 0) || (SampleSequence == NULL)) return;

			// Otherwise step back
			if(SequencePos == 0) SequencePos = SampleSequenceSize - 1;
			else SequencePos--;
		}

		//! Get BytesPerEditUnit for a specified number of sample bytes (take into account KAG and K + L)
		UInt32 GetBPE_Internal(UInt32 KAGSize, UInt32 SampleSize);

		//! Read the sequence header at the specified position in an MPEG2 file to build an essence descriptor
		MDObjectPtr BuildWaveAudioDescriptor(FileHandle InFile, UInt64 Start = 0);

		//! Scan the essence to calculate how many bytes to transfer for the given edit unit count
		size_t ReadInternal(FileHandle InFile, UInt32 Stream, UInt64 Count);
	};
}

#endif // MXFLIB__ESP_WAVEPCM_H
