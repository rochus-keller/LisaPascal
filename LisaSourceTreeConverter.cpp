/****************************************************************************
 *
 *  Apple Lisa OS Source Preparation and Patching Tool
 *  Integration for the Lisa Code Navigator
 *
 *  Description:
 *  This utility processes the raw 1983 Apple Lisa OS source code repository
 *  (as released by the Computer History Museum in 2023). It automatically
 *  restructures the directory tree, corrects file encodings and line endings,
 *  and applies necessary source-level patches (grafting and text replacements)
 *  to make the code consistent and compilable.
 *
 *  Credits & History:
 *  - Original Concept & Python Implementation:
 *    By AlexTheCat123 (Alex Anderson-McLeod), 2025
 *    Based on his reverse-engineering of the Lisa build process and his
 *    original Python scripts (process_source.py, singlefile_cr.py, and
 *    patch_files.py) from the "LOS Compilation Base" project, see
 *    https://github.com/alexthecat123/LisaSourceCompilation.
 *
 *  - This is a C++98/Qt5 version of the mentioned Python scripts
 *    by me@rochus-keller.ch, 2026.
 *
 *  License: GNU General Public License v3 (GPLv3)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 ****************************************************************************/

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QTextStream>
#include <QVector>

struct PatchDef {
    QString filePath; // The translated Lisa path
    QString searchStr;
    QString replaceStr;
    int expectedCount;

    PatchDef() : expectedCount(0) {}
    PatchDef(const QString& path, const QString& search, const QString& replace, int count)
        : filePath(path.toLower()), searchStr(search), replaceStr(replace), expectedCount(count) {}
};

QTextStream out(stdout);

static inline QString txt(const char* str)
{
    return QString::fromLatin1(str).toLower();
}

void phase1_CopyAndConvert(const QString& inDir, const QString& outDir) {
    out << "Phase 1: Copying, restructuring, and converting encodings...\n";

    QStringList badPaths;
    badPaths << txt("DICT") << txt("LISA_OS/APIN") << txt("LISA_OS/BUILD") << txt("LISA_OS/FONTS")
             << txt("LISA_OS/LIBHW") << txt("LISA_OS/LINKMAPS 3.0") << txt("LISA_OS/LINKMAPS AND MISC. 3.0")
             << txt("LISA_OS/OS EXEC FILES") << txt("LISA_TOOLKIT");

    QStringList validPaths;
    validPaths << txt("BUILD/BUILD-COMP") << txt("BUILD/BUILD-ASSEMB") << txt("BUILD/BUILD-INSTALL") << txt("TKALERT");

    QDirIterator it(inDir, QDirIterator::Subdirectories);
    int filesProcessed = 0;

    while (it.hasNext()) {
        const QString srcPath = it.next();
        const QFileInfo fi(srcPath);
        const QString origPath = fi.fileName();
        
        if (origPath.endsWith(".unix.txt", Qt::CaseInsensitive) || origPath.endsWith(".text", Qt::CaseInsensitive)) {
            QString fullPathLower = fi.absoluteFilePath().toLower();
            
            // Apply Alex's Blocklist logic
            bool isBad = false;
            for (int i = 0; i < badPaths.size(); ++i) {
                if (fullPathLower.contains(badPaths[i])) {
                    isBad = true;
                    break;
                }
            }
            bool isValid = false;
            for (int i = 0; i < validPaths.size(); ++i) {
                if (fullPathLower.contains(validPaths[i])) {
                    isValid = true;
                    break;
                }
            }
            if (isBad && !isValid)
                continue;

            // Apply Alex's 4-step string manipulation trick on the basename
            QString newPath = origPath.toLower();
            newPath.replace(".unix.txt", "");
            newPath.replace(".", "/");
            newPath.replace("-", "/");
            if (newPath.length() > 5) {
                QString ext = newPath.right(4);
                newPath = newPath.left(newPath.length() - 5) + "." + ext;
            }

            const QString destPath = outDir + "/" + newPath;

            // Read the raw bytes and apply the serial_transfer data parsing logic
            QFile inFile(srcPath);
            if (inFile.open(QIODevice::ReadOnly)) {
                QByteArray raw = inFile.readAll();
                inFile.close();

                // Truncate at the first 0xFF byte
                int ffIndex = raw.indexOf('\xFF');
                if (ffIndex != -1) {
                    raw.truncate(ffIndex);
                }

                // in contrast to Alex we don't convert line endings but keep the Unix format

                // Write to the new structured directory
                QFileInfo destFi(destPath);
                destFi.dir().mkpath("."); // Ensure destination folder exists

                QFile outFile(destPath);
                if (outFile.open(QIODevice::WriteOnly)) {
                    outFile.write(raw);
                    outFile.close();
                    filesProcessed++;
                }
            }
        }
    }
    out << "  Processed and fixed " << filesProcessed << " files.\n\n";
}

void extractAndPatch(const QString& srcContent, const QString& startTag, const QString& endTag, const QString& destPath) {
    int startIdx = srcContent.indexOf(startTag);
    int endIdx = srcContent.indexOf(endTag, startIdx);
    if (startIdx != -1 && endIdx != -1) {
        QString extracted = srcContent.mid(startIdx, endIdx - startIdx);
        QFile outFile(destPath);
        if (outFile.open(QIODevice::WriteOnly)) {
            QString finalContent = extracted + "       .END\n";
            outFile.write(finalContent.toLatin1());
            outFile.close();
        }
    }
}

void patchFiles(const QString& outDir) {
    out << "Phase 2: File Patching...\n";
    int patches = 0;

    QDir().mkpath(outDir + txt("/LIBPL"));

    // 1. PASMATH.TEXT
    if (QFile::exists(outDir + txt("/SOURCE/PASMATH.TEXT")) && !QFile::exists(outDir + txt("/LIBPL/PASMATH.TEXT"))) {
        QFile::copy(outDir + txt("/SOURCE/PASMATH.TEXT"), outDir + txt("/LIBPL/PASMATH.TEXT"));
        out << "  Patched LIBPL/PASMATH.TEXT\n";
        patches++;
    }

    // 2. PASMOVE, PASMISC, PASRANGE
    QFile osintFile(outDir + txt("/SOURCE/OSINTPASLIB.TEXT"));
    if (osintFile.open(QIODevice::ReadOnly)) {
        QString osintContent = QString::fromLatin1(osintFile.readAll());
        osintFile.close();

        if (!QFile::exists(outDir + txt("/LIBPL/PASMOVE.TEXT"))) {
            extractAndPatch(osintContent, "; File: PASMOVE.TEXT", "; File: PASRANGE.TEXT", outDir + txt("/LIBPL/PASMOVE.TEXT"));
            out << "  Patched LIBPL/PASMOVE.TEXT\n";
            patches++;
        }
        if (!QFile::exists(outDir + txt("/LIBPL/PASMISC.TEXT"))) {
            extractAndPatch(osintContent, "; File: PASMISC.TEXT", "; File: PASMOVE.TEXT", outDir + txt("/LIBPL/PASMISC.TEXT"));
            out << "  Patched LIBPL/PASMISC.TEXT\n";
            patches++;
        }
        if (!QFile::exists(outDir + txt("/LIBPL/PASRANGE.TEXT"))) {
            extractAndPatch(osintContent, "; File: PASRANGE.TEXT", "; File: PASSCOMP.TEXT", outDir + txt("/LIBPL/PASRANGE.TEXT"));
            out << "  Patched LIBPL/PASRANGE.TEXT\n";
            patches++;
        }
    }

    // 3. SYSCALL.TEXT
    QFile sysFile(outDir + txt("/LIBOS/SYSCALL.TEXT"));
    if (sysFile.open(QIODevice::ReadOnly)) {
        QString content = QString::fromLatin1(sysFile.readAll());
        sysFile.close();
        if (!content.contains("Copyright 1983, 1984, Apple Computer Inc.")) {
            QFile::remove(outDir + txt("/LIBOS/SYSCALL.TEXT"));
            QFile::copy(outDir + txt("/SOURCE/SYSCALL.TEXT"), outDir + txt("/LIBOS/SYSCALL.TEXT"));
            out << "  Replaced LIBOS/SYSCALL.TEXT\n";
            patches++;
        }
    }

    // 4. UNITLOTUS.TEXT definitions
    QFile lotusFile(outDir + txt("/APLW/UNITLOTUS.TEXT"));
    if (lotusFile.open(QIODevice::ReadOnly)) {
        QString content = QString::fromLatin1(lotusFile.readAll());
        lotusFile.close();
        if (!content.contains("TSpReturn = (ok,")) {
            content.replace("CONST dxScroll = 16;",
                            "\nTYPE\n    TSpReturn = (ok, notInitialized, illegalString, masterError, unableToLoad, userMemoryFull, wordExists, notFound, limitExceeded);\n\nCONST dxScroll = 16;");
            if (lotusFile.open(QIODevice::WriteOnly)) {
                lotusFile.write(content.toLatin1());
                lotusFile.close();
                out << "  Patched headers in APLW/UNITLOTUS.TEXT\n";
                patches++;
            }
        }
    }
    
    out << "  Completed " << patches << " file patches.\n\n";
}

void phase3_ApplyPatches(const QString& outDir) {
    out << "Phase 3: Applying Source Code Patches...\n";

    QVector<PatchDef> p;
    // we keep the Unix \n
    p.push_back(PatchDef("APIM/IMEVTLOOP.TEXT", "{$U apim/tfilercomm }tfilercomm", "{$U FilerComm }  FilerComm", 1));
    p.push_back(PatchDef("APIM/IMFOLDERS.TEXT", "ObjectKind", "ObjectAKind", 1));
    p.push_back(PatchDef("APIM/IMINTERP.TEXT", "ObjectKind", "ObjectAKind", 3));
    p.push_back(PatchDef("APIM/IMINTERP.TEXT", "{$U fld.obj      }  FieldEdit,", "{$U fld.obj     }  FieldEdit,\n     {$U FilerComm } FilerComm,", 1));
    p.push_back(PatchDef("APIM/IMPATMAT.TEXT", "{$U fld.obj      }  FieldEdit,", "{$U fld.obj     }  FieldEdit,\n     {$U FilerComm } FilerComm,", 1));
    p.push_back(PatchDef("APIM/IMSCRIPT.TEXT", "{$U fld.obj      }  FieldEdit,", "{$U fld.obj     }  FieldEdit,\n     {$U FilerComm } FilerComm,", 1));
    p.push_back(PatchDef("APIM/IMSIM.TEXT", "{$U fld.obj      }  FieldEdit,", "{$U fld.obj     }  FieldEdit,\n     {$U FilerComm } FilerComm,", 1));
    p.push_back(PatchDef("APIM/TCATALOG.TEXT", "nilKind     = 0;", "{nilKind    = 0;", 1));
    p.push_back(PatchDef("APIM/TCATALOG.TEXT", "fileKind    = 1;", "fileKind   = 1;}", 1));
    p.push_back(PatchDef("APIM/TCATALOG.TEXT", "folderkind  = 3;", "{folderkind = 3;", 1));
    p.push_back(PatchDef("APIM/TCATALOG.TEXT", "computerKind= 10;", "computerKind=10;}", 1));
    p.push_back(PatchDef("APIM/TCATALOG.TEXT", "folderPad   = 16;", "{folderPad  = 16;}", 1));
    p.push_back(PatchDef("APIM/TCATALOG.TEXT", "clockKind   = 19;", "{clockKind  = 19;}", 1));
    p.push_back(PatchDef("APIM/TCATALOG.TEXT", "toolKind    = 24;", "{toolKind   = 24;", 1));
    p.push_back(PatchDef("APIM/TCATALOG.TEXT", "disk2Kind   = 27;", "disk2Kind  = 27;}", 1));
    p.push_back(PatchDef("APIM/TCATALOG.TEXT", "drawerKind", "actualDrawerKind", 1));
    p.push_back(PatchDef("APIM/TCATALOG.TEXT", "deskKind", "actualDeskKind", 2));
    p.push_back(PatchDef("APIM/TCATALOG.TEXT", "profileKind", "actualProfileKind", 2));
    p.push_back(PatchDef("APIM/TCATALOG.TEXT", "lastKind", "actualLastKind", 1));
    p.push_back(PatchDef("APIM/TCATALOG.TEXT", "{$U AlertMgr  } AlertMgr,", "{$U AlertMgr   } AlertMgr,\n   {$U FilerComm } FilerComm,", 1));
    p.push_back(PatchDef("APIM/TFDOCCTRL.TEXT", "{$U apim/tfilercomm } tfilercomm", "{$U FilerComm }  FilerComm", 1));
    p.push_back(PatchDef("APIM/TFDOCCTRL.TEXT", "profileKind", "actualProfileKind", 2));
    p.push_back(PatchDef("APIM/TFILER.TEXT", "{$U Apim/tfilercomm }  Tfilercomm", "{$U FilerComm }  FilerComm", 1));
    p.push_back(PatchDef("APIM/TFILER.TEXT", "drawerKind", "actualDrawerKind", 3));
    p.push_back(PatchDef("APIM/TFILER.TEXT", "deskKind", "actualDeskKind", 3));
    p.push_back(PatchDef("APIM/TFILER.TEXT", "profileKind", "actualProfileKind", 7));
    p.push_back(PatchDef("APIM/TFILER.TEXT", "lastKind", "actualLastKind", 4));
    p.push_back(PatchDef("APIM/TFILER.TEXT", "iconWidth    = 48;", "{iconWidth   = 48;}", 1));
    p.push_back(PatchDef("APIM/TFILER.TEXT", "iconHt       = 32;", "{iconHt      = 32;}", 1));
    p.push_back(PatchDef("APIM/TFILER.TEXT", "ObjectKind", "ObjectAKind", 20));
    p.push_back(PatchDef("APIM/TFILER.TEXT", "objectKind", "ObjectAKind", 2));
    p.push_back(PatchDef("APIM/TFILER.TEXT", "iconData", "aIconData", 32));
    p.push_back(PatchDef("APIM/TFILER.TEXT", "iconMask", "aIconMask", 34));
    p.push_back(PatchDef("APIM/TFILER.TEXT", "iconBoxes", "aIconBoxes", 3));
    p.push_back(PatchDef("APIM/TFILER2.TEXT", "deskKind", "actualDeskKind", 1));
    p.push_back(PatchDef("APIM/TFILER2.TEXT", "profileKind", "actualProfileKind", 3));
    p.push_back(PatchDef("APIM/TFILER2.TEXT", "ObjectKind", "ObjectAKind", 6));
    p.push_back(PatchDef("APIM/TFILER2.TEXT", "iconData", "aIconData", 1));
    p.push_back(PatchDef("APIM/TFILER2.TEXT", "iconBoxes", "aIconBoxes", 2));
    p.push_back(PatchDef("APIM/TFILERINT.TEXT", "drawerKind", "actualDrawerKind", 1));
    p.push_back(PatchDef("APIM/TFILERINT.TEXT", "deskKind", "actualDeskKind", 2));
    p.push_back(PatchDef("APIM/TFILERINT.TEXT", "profileKind", "actualProfileKind", 1));
    p.push_back(PatchDef("APIM/TFILERINT.TEXT", "ObjectKind", "ObjectAKind", 4));
    p.push_back(PatchDef("APLL/INITFEX.TEXT", "{t5}", "{t103}", 2));
    p.push_back(PatchDef("APLT/INIT.TEXT", "{T10}", "{T104}", 2));
    p.push_back(PatchDef("APLW/UNITLOTUS.TEXT", "{$U ApLW/UnitSpell.Obj }  UnitSpell,", "{ ApLW/UnitSpell.Obj   UnitSpell, }", 1));
    p.push_back(PatchDef("APLW/UNITLOTUS.TEXT", "{$u aplw/sp/spelling.obj }    spelling,", "{ aplw/sp/spelling.obj     spelling, }", 1));
    p.push_back(PatchDef("APLW/UNITLOTUS.TEXT", "{$u aplw/sp/verify.obj }      SpVerify,", "{ aplw/sp/verify.obj       SpVerify, }", 1));
    p.push_back(PatchDef("APLW/UNITLOTUS.TEXT", "UndoPutDict(fUndoUndo);", "{UndoPutDict(fUndoUndo)}", 1));
    p.push_back(PatchDef("APLW/UNITLOTUS.TEXT", "UndoRmvDict(fUndoUndo);", "{UndoRmvDict(fUndoUndo)}", 1));
    p.push_back(PatchDef("APLW/UNITLOTUS.TEXT", "DoSpellImid(imid, orecConDwn);", "{DoSpellImid(imid,  orecConDwn);}", 1));
    p.push_back(PatchDef("APLW/UNITLOTUS.TEXT", "imidGuess: EndGuessDbox(butnReplace);", "{imidGuess:  EndGuessDbox(butnReplace);}", 1));
    p.push_back(PatchDef("APLW/UNITLOTUS.TEXT", "imidGNext: EndGuessDbox(butnRNext);", "{imidGNext:  EndGuessDbox(butnRNext);}", 1));
    p.push_back(PatchDef("APLW/UNITLOTUS.TEXT", "NQMore;", "{NQMore}\n            fMoreToQ := FALSE;", 1));
    p.push_back(PatchDef("APLW/UNITLOTUS.TEXT", "ParamAlert(ShowReturn (ord (HMReturn))^, '', '');", "{ParamAlert(ShowReturn (ord(HMReturn))^, '', '');}", 1));
    p.push_back(PatchDef("APLW/UNITLOTUS.TEXT", "SetHzSpell(hzLotus);", "{SetHzSpell(hzLotus)}\ntheDBox.isOpen := FALSE;", 1));
    p.push_back(PatchDef("APLW/UNITLOTUS.TEXT", "SpTerminate(closeFile)", "ok", 1));
    p.push_back(PatchDef("APLW/UNITLOTUS.TEXT", "fGenOrec: TF;", "fGenOrec:  TF;\n    fTstSpell: TF;\n    fTstHeap: TF;\n    fMoreToQ: TF;", 1));
    p.push_back(PatchDef("APLW/UNITLOTUS.TEXT", "{T1}", "{T105}", 2));
    p.push_back(PatchDef("APLW/TESTBOX.TEXT", "{$U ApLW/UnitSpell.Obj }  UnitSpell,", "{ ApLW/UnitSpell.Obj   UnitSpell, }", 1));
    p.push_back(PatchDef("APLW/TESTBOX.TEXT", "{$u aplw/sp/spelling.obj }    spelling,", "{ aplw/sp/spelling.obj     spelling, }", 1));
    p.push_back(PatchDef("APLW/TESTBOX.TEXT", "{$u aplw/sp/verify.obj }      SpVerify,", "{ aplw/sp/verify.obj       SpVerify, }", 1));
    p.push_back(PatchDef("APLW/TESTBOX.TEXT", "fTstSpell := f;", "{fTstSpell :=  f;}", 1));
    p.push_back(PatchDef("APLW/TESTBOX.TEXT", "fTstHeap := f;", "{fTstHeap :=  f;}", 1));
    p.push_back(PatchDef("APLW/TESTBOX.TEXT", "fTrash := FCheckFlag('Spell', fTstSpell);", "{fTrash := FCheckFlag('Spell',  fTstSpell);}", 1));
    p.push_back(PatchDef("APLW/TESTBOX.TEXT", "fTrash := FCheckFlag('Heap', fTstHeap);", "{fTrash := FCheckFlag('Heap',  fTstHeap);}", 1));
    p.push_back(PatchDef("APLW/TESTBOX.TEXT", "writeln('SpTerminate returned ', ShowReturn (ord (HMReturn))^);", "{writeln('SpTerminate returned ', ShowReturn (ord(HMReturn))^);}", 1));
    p.push_back(PatchDef("APLW/TESTBOX.TEXT", "spStatus := uninitialized;", "{spStatus :=  uninitialized;}", 1));
    p.push_back(PatchDef("APLW/TESTBOX.TEXT", "SpTerminate(cleanUp)", "ok", 1));
    p.push_back(PatchDef("APLW/TESTBOX.TEXT", "LABEL 999;", "LABEL  999;\n\nTYPE\n    TSpReturn = (ok, notInitialized, illegalString, masterError, unableToLoad, userMemoryFull, wordExists, notFound, limitExceeded);", 1));
    p.push_back(PatchDef("APLW/SP/VERIFY.TEXT", "{t1}", "{t105}", 1));
    p.push_back(PatchDef("APLW/UNITSCRAP.TEXT", "{t1}", "{t105}", 1));
    p.push_back(PatchDef("APLW/UNITSCRAP.TEXT", "{T1}", "{T105}", 2));
    p.push_back(PatchDef("APLW/UNITSPELL.TEXT", "{t1}", "{t105}", 1));
    p.push_back(PatchDef("APLW/UNITSPELL.TEXT", "{T1}", "{T105}", 2));
    p.push_back(PatchDef("APLW/UNITSRCH.TEXT", "{T1}", "{T105}", 1));
    p.push_back(PatchDef("APLC/LCFILER.TEXT", "{T2}", "{T108}", 1));
    p.push_back(PatchDef("APLC/LCFILER.TEXT", "{T3}", "{T107}", 1));
    p.push_back(PatchDef("APLC/MM/LEX.TEXT", "{T2}", "{T108}", 1));
    p.push_back(PatchDef("APLC/MM/LEX.TEXT", "{T3}", "{T107}", 1));
    p.push_back(PatchDef("APLC/APPDIBOX.TEXT", "{T3}", "{T107}", 5));
    p.push_back(PatchDef("APLC/APPDIBOX.TEXT", "inPutGrahics", "inPutGraphics", 1));
    p.push_back(PatchDef("APBG/BG1.TEXT", "BG1.0", "BG1/0", 3));
    p.push_back(PatchDef("APPW/BTNREAD.TEXT", "{T11}buttons", "APPW/T11buttons", 1));
    p.push_back(PatchDef("BUILD/COMP.TEXT", "-newdisk-intrinsic.lib", "intrinsic.lib", 3));
    p.push_back(PatchDef("BUILD/INSTALL.TEXT", "-newdisk-intrinsic.lib", "intrinsic.lib", 5));
    p.push_back(PatchDef("LIBDB/LMSCAN.TEXT", "{$SETC OSBUILT := TRUE }", "{$SETC OSBUILT := TRUE  }\n{$SETC fSymOk := FALSE  }\n{$SETC fTRACE := FALSE  }", 1));
    p.push_back(PatchDef("LIBDB/LMSCAN.TEXT", "procedure diffWAdDelete", "procedure diffWADelete", 1));
    p.push_back(PatchDef("LIBFE/FLD/MAIN.TEXT", "{$I libfe/fld.fmt.text}", "{$I libfe/fld/fmt.text }", 1));
    p.push_back(PatchDef("LIBFE/FLD/MAIN.TEXT", "{$I libfe/fld.disp.text}", "{$I libfe/fld/disp.text }", 1));
    p.push_back(PatchDef("LIBFE/FLD/MAIN.TEXT", "{$I libfe/fld.sel.text}", "{$I libfe/fld/sel.text }", 1));
    p.push_back(PatchDef("LIBFE/FLD/MAIN.TEXT", "{$I libfe/fld.word.text}", "{$I libfe/fld/word.text }", 1));
    p.push_back(PatchDef("LIBFE/FLD/MAIN.TEXT", "{$I libfe/fld.edit.text}", "{$I libfe/fld/edit.text }", 1));
    p.push_back(PatchDef("LIBFE/FLD/MAIN.TEXT", "{$I libfe/fld.cmds.text}", "{$I libfe/fld/cmds.text }", 1));
    p.push_back(PatchDef("LIBFE/FLD/MAIN.TEXT", "{$I libfe/fld.font.text}", "{$I libfe/fld/font.text }", 1));
    p.push_back(PatchDef("LIBFP/NEWFPLIB.TEXT", "{$I libFP/str2dec }", "procedure Str2Dec; external;", 1));
    p.push_back(PatchDef("LIBHW/KEYBOARD.TEXT", "uses {$U hwint.obj} LibHW/hwint;", "uses {$U libhw/hwint.obj} hwint;", 1));
    p.push_back(PatchDef("LIBOS/PSYSCALL.TEXT", "(*$U object/syscall.obj *)", "(*$U libos/syscall.obj *)", 1));
    p.push_back(PatchDef("LIBPL/TFLDERCALL.TEXT", " paslibequs.text", " libpl/paslibequs.text", 1));
    p.push_back(PatchDef("LIBQD/QDSAMPLE.TEXT", "$U obj:QuickDraw", "$U LIBQD/QuickDraw", 1));
    p.push_back(PatchDef("LIBQD/QDSAMPLE.TEXT", "$U obj:QDSupport", "$U LIBQD/QDSupport", 1));
    p.push_back(PatchDef("LIBQP/UBAUDRATE.TEXT", "{$U -newdisk-QP/Hardware} Hardware;", "{$U LIBQP/QP/Hardware} Hardware;", 1));
    p.push_back(PatchDef("LIBTK/UTEXT.TEXT", "{$U UABC}", "{$U libtk/UABC}", 1));
    p.push_back(PatchDef("TKIN/SOURCE.TEXT", "{$U Tkin/Globals", "{$U APDM/Globals", 1));
    p.push_back(PatchDef("TKIN/SOURCE.TEXT", "{$U Tkin/Cat", "{$U APDM/Cat", 1));
    p.push_back(PatchDef("TKIN/ENTRY.TEXT", "{$U TKIN/Globals}", "{$U APDM/Globals}", 1));
    p.push_back(PatchDef("SOURCE/PROFILE.TEXT", "if (discsize <= 9728) or (discsize > 30000)", "if (discsize <= 9728) or (discsize > 500000)", 1));
    p.push_back(PatchDef("SOURCE/DRIVERDEFS.TEXT", "(*$SETC DEBUG1:=TRUE*)", "(*$SETC DEBUG1:=FALSE*)", 1));
    p.push_back(PatchDef("SOURCE/DRIVERDEFS.TEXT", "(*$SETC TWIGGYBUILD:=TRUE*)", "(*$SETC TWIGGYBUILD:=FALSE*)", 1));
    p.push_back(PatchDef("SOURCE/PASCALDEFS.TEXT", "DEBUG1          .EQU    1", "DEBUG1          .EQU    0", 1));
    p.push_back(PatchDef("SOURCE/PASCALDEFS.TEXT", "TWIGGYBUILD     .EQU    1", "TWIGGYBUILD     .EQU    0", 1));
    p.push_back(PatchDef("LIBPL/PASMATH.TEXT", "        .END", "\n        .include libpl/pwrii.text\n\n       .END", 1));
    p.push_back(PatchDef("LIBPL/PASMISC.TEXT", ".PROC   %%%MISC", ".PROC    %%%MISC\n\n        .include libpl/paslibdefs.text", 1));
    p.push_back(PatchDef("LIBPL/PASMISC.TEXT", ".ref    gotoxy", ".ref    %_FGOTOXY", 1));
    p.push_back(PatchDef("LIBPL/PASMISC.TEXT", "jsr     gotoxy", "jsr     %_FGOTOXY", 1));

    int applied = 0;
    int expectedTotal = 0;

    for (int i = 0; i < p.size(); ++i) {
        expectedTotal += p[i].expectedCount;
    }

    // Apply patches grouped by file
    QStringList uniqueFiles;
    for (int i = 0; i < p.size(); ++i) {
        if (!uniqueFiles.contains(p[i].filePath)) {
            uniqueFiles.push_back(p[i].filePath);
        }
    }

    for (int fIdx = 0; fIdx < uniqueFiles.size(); ++fIdx) {
        QString filePath = outDir + "/" + uniqueFiles[fIdx];
        QFile file(filePath);
        
        if (file.open(QIODevice::ReadOnly)) {
            QString content = QString::fromLatin1(file.readAll());
            file.close();
            bool modified = false;

            for (int pIdx = 0; pIdx < p.size(); ++pIdx) {
                if (p[pIdx].filePath == uniqueFiles[fIdx]) {
                    int foundCount = content.count(p[pIdx].searchStr);
                    if (foundCount > 0) {
                        content.replace(p[pIdx].searchStr, p[pIdx].replaceStr);
                        applied += foundCount;
                        modified = true;
                    }
                    if (foundCount != p[pIdx].expectedCount) {
                        out << "    WARNING: Patched " << foundCount << " instances of \"" 
                            << p[pIdx].searchStr << "\" (Expected " << p[pIdx].expectedCount << ")\n";
                    }
                }
            }

            if (modified) {
                if (file.open(QIODevice::WriteOnly)) {
                    file.write(content.toLatin1());
                    file.close();
                }
            }
        } else {
            out << "  WARNING: Failed to find file " << uniqueFiles[fIdx] << " for patching.\n";
        }
    }

    out << "  Successfully applied " << applied << "/" << expectedTotal << " patches.\n\n";
}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    if (argc < 3) {
        out << "Usage: convert <path_to_raw_chm_source> <path_to_output_dir>\n";
        out << "Example: convert ./Lisa_Source ./Lisa_Source_Restructured\n";
        return 1;
    }

    QString inDir = QDir::cleanPath(QString::fromLocal8Bit(argv[1]));
    QString outDir = QDir::cleanPath(QString::fromLocal8Bit(argv[2]));

    QDir dir(inDir);
    if (!dir.exists()) {
        out << "Error: Input directory does not exist!\n";
        return 1;
    }

    phase1_CopyAndConvert(inDir, outDir);
    patchFiles(outDir);
    phase3_ApplyPatches(outDir);

    out << "Mission complete.\n";
    return 0;
}
