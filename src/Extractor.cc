/*
 * steghide 0.5.1 - a steganography program
 * Copyright (C) 1999-2003 Stefan Hetzl <shetzl@chello.at>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#include <string>

#include "BitString.h"
#include "CvrStgFile.h"
#include "EmbData.h"
#include "Extractor.h"
#include "SampleValue.h"
#include "Selector.h"
#include "common.h"
#include "error.h"

EmbData *Extractor::extract() {
    if (Args.StgFn.getValue() == "") {
        VerboseMessage::print("reading stego file from standard input...");
    } else {
        VerboseMessage::print("reading stego file \"%s\"...", Args.StgFn.getValue().c_str());
    }

    Globs.TheCvrStgFile = CvrStgFile::readFile(StegoFileName);

    VerboseMessage::printRaw("done.\n");

    Selector *sel;
    EmbData *embdata;

    // Determine wether we are cracking with or without a passphrase
    if (passphraseSet) {
        embdata = new EmbData(EmbData::EXTRACT, Passphrase);
        sel = new Selector(Globs.TheCvrStgFile->getNumSamples(), Passphrase);
    } else {
        // The password is only used for encrypted data.
        // We can still extract plain text using only a seed value
        embdata = new EmbData(EmbData::EXTRACT, "");
        sel = new Selector(Globs.TheCvrStgFile->getNumSamples(), seed);
    }

    VerboseMessage::print("extracting data...");

    unsigned long sv_idx = 0;
    while (!embdata->finished()) {
        unsigned short bitsperembvalue =
            AUtils::log2_ceil<unsigned short>(Globs.TheCvrStgFile->getEmbValueModulus());
        unsigned long embvaluesrequested =
            AUtils::div_roundup<unsigned long>(embdata->getNumBitsRequested(), bitsperembvalue);
        if (sv_idx + (Globs.TheCvrStgFile->getSamplesPerVertex() * embvaluesrequested) >=
            Globs.TheCvrStgFile->getNumSamples()) {
            if (Globs.TheCvrStgFile->is_std()) {
                throw CorruptDataError("the stego data from standard input is too "
                                       "short to contain the embedded data.");
            } else {
                throw CorruptDataError("the stego file \"%s\" is too short to "
                                       "contain the embedded data.",
                                       Globs.TheCvrStgFile->getName().c_str());
            }
        }
        BitString bits(Globs.TheCvrStgFile->getEmbValueModulus());
        for (unsigned long i = 0; i < embvaluesrequested; i++) {
            EmbValue ev = 0;
            for (unsigned int j = 0; j < Globs.TheCvrStgFile->getSamplesPerVertex();
                 j++, sv_idx++) {
                ev = (ev + Globs.TheCvrStgFile->getEmbeddedValue((*sel)[sv_idx])) %
                     Globs.TheCvrStgFile->getEmbValueModulus();
            }
            bits.appendNAry(ev);
        }
        embdata->addBits(bits);
    }

    delete sel;

    VerboseMessage::printRaw("done.\n");

    // TODO (postponed due to message freeze): rename into "verifying crc32
    // checksum..."
    VerboseMessage::print("checking crc32 checksum...");
    if (embdata->checksumOK()) {
        VerboseMessage::printRaw(" ok\n");
    } else {
        VerboseMessage::printRaw(" FAILED\n");
        CriticalWarning::print("crc32 checksum failed! extracted data is probably corrupted.\n");
    }
    return embdata;
}
