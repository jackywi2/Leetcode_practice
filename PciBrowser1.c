// PciBrowser.c
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/BaseLib.h>
#include <Library/SortLib.h>
#include <Protocol/PciIo.h>
#include <Protocol/SimpleTextInEx.h>

#define PAGE_LINES        20
#define MAX_CLASS_NAME    32

typedef struct {
  EFI_HANDLE                 Handle;
  EFI_PCI_IO_PROTOCOL       *PciIo;
  UINTN                     Segment;
  UINTN                      Bus;
  UINTN                      Device;
  UINTN                      Function;
  UINT16                     VendorId;
  UINT16                     DeviceId;
  UINT8                      BaseClass;
  UINT8                      SubClass;
  UINT8                      ProgIf;
} PCI_DEVICE_ENTRY;

STATIC EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *gTextInEx = NULL;

// ----- Small class decoder (base/sub only, brief) -----
STATIC CONST CHAR16* ClassName(UINT8 BaseClass, UINT8 SubClass) {
  switch (BaseClass) {
    case 0x00: return L"Unclassified";
    case 0x01: switch (SubClass) {
      case 0x00: return L"Mass Storage (SCSI)";
      case 0x01: return L"Mass Storage (IDE)";
      case 0x06: return L"Mass Storage (SATA)";
      case 0x08: return L"Mass Storage (NVMe)";
      default:   return L"Mass Storage";
    }
    case 0x02: return L"Network Controller";
    case 0x03: switch (SubClass) {
      case 0x00: return L"VGA Compatible";
      case 0x02: return L"3D Controller";
      default:   return L"Display Controller";
    }
    case 0x04: return L"Multimedia Controller";
    case 0x05: return L"Memory Controller";
    case 0x06: return L"Bridge Device";
    case 0x07: return L"Simple Comm Controller";
    case 0x08: return L"Base System Peripheral";
    case 0x09: return L"Input Device";
    case 0x0A: return L"Docking Station";
    case 0x0B: return L"Processor";
    case 0x0C: return L"Serial Bus Controller";
    case 0x0D: return L"Wireless Controller";
    case 0x0E: return L"Intelligent Controller";
    case 0x0F: return L"Satellite Comm";
    case 0x10: return L"Encryption/Decryption";
    case 0x11: return L"Data Acquisition/Signal";
    default:    return L"Other";
  }
}

// ----- Helpers -----
STATIC
INTN CmpPci(const VOID *A, const VOID *B) {
  const PCI_DEVICE_ENTRY *pa = (const PCI_DEVICE_ENTRY *)A;
  const PCI_DEVICE_ENTRY *pb = (const PCI_DEVICE_ENTRY *)B;
  if (pa->Segment != pb->Segment) return (pa->Segment < pb->Segment) ? -1 : 1;
  if (pa->Bus     != pb->Bus)     return (pa->Bus     < pb->Bus)     ? -1 : 1;
  if (pa->Device  != pb->Device)  return (pa->Device  < pb->Device)  ? -1 : 1;
  if (pa->Function!= pb->Function)return (pa->Function< pb->Function)? -1 : 1;
  return 0;
}

STATIC
VOID WaitForKey(VOID) {
  if (gTextInEx) {
    EFI_KEY_DATA kd;
    while (gTextInEx->ReadKeyStrokeEx(gTextInEx, &kd) == EFI_SUCCESS) { /* flush */ }
    gBS->WaitForEvent(1, &gTextInEx->WaitForKeyEx, NULL);
  } else {
    EFI_INPUT_KEY k;
    while (gST->ConIn->ReadKeyStroke(gST->ConIn, &k) == EFI_SUCCESS) { /* flush */ }
    gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, NULL);
  }
}

STATIC
BOOLEAN ReadKey(EFI_INPUT_KEY *Key, UINT16 *ScanCode) {
  if (gTextInEx) {
    EFI_KEY_DATA kd;
    if (gTextInEx->ReadKeyStrokeEx(gTextInEx, &kd) == EFI_SUCCESS) {
      if (Key) *Key = kd.Key;
      if (ScanCode) *ScanCode = kd.Key.ScanCode;
      return TRUE;
    }
  } else {
    EFI_INPUT_KEY k;
    if (gST->ConIn->ReadKeyStroke(gST->ConIn, &k) == EFI_SUCCESS) {
      if (Key) *Key = k;
      if (ScanCode) *ScanCode = k.ScanCode;
      return TRUE;
    }
  }
  return FALSE;
}

STATIC
UINTN ReadHexFromConsole(CONST CHAR16 *Prompt, UINTN DefaultVal, BOOLEAN *Aborted) {
  CHAR16 buf[32];
  UINTN len = 0;
  if (Aborted) *Aborted = FALSE;
  Print(L"%s [hex] (Enter=0x%lx, ESC=abort): ", Prompt, DefaultVal);
  while (1) {
    EFI_INPUT_KEY k; UINT16 s;
    WaitForKey();
    if (!ReadKey(&k, &s)) continue;
    if (k.UnicodeChar == CHAR_CARRIAGE_RETURN) break;
    if (s == SCAN_ESC) { if (Aborted) *Aborted = TRUE; Print(L"\n"); return DefaultVal; }
    if (k.UnicodeChar == CHAR_BACKSPACE) {
      if (len) { buf[--len] = 0; Print(L"\b \b"); }
      continue;
    }
    CHAR16 c = k.UnicodeChar;
    if ((c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f') || (c >= L'A' && c <= L'F')) {
      if (len < 31) {
        buf[len++] = c; buf[len] = 0;
        Print(L"%c", c);
      }
    }
  }
  Print(L"\n");
  if (len == 0) return DefaultVal;
  return StrHexToUintn(buf);
}

STATIC
UINTN ReadDecFromConsole(CONST CHAR16 *Prompt, UINTN DefaultVal, BOOLEAN *Aborted) {
  CHAR16 buf[16]; UINTN len = 0;
  if (Aborted) *Aborted = FALSE;
  Print(L"%s (Enter=%u, ESC=abort): ", Prompt, DefaultVal);
  while (1) {
    EFI_INPUT_KEY k; UINT16 s;
    WaitForKey();
    if (!ReadKey(&k, &s)) continue;
    if (k.UnicodeChar == CHAR_CARRIAGE_RETURN) break;
    if (s == SCAN_ESC) { if (Aborted) *Aborted = TRUE; Print(L"\n"); return DefaultVal; }
    if (k.UnicodeChar == CHAR_BACKSPACE) {
      if (len) { buf[--len] = 0; Print(L"\b \b"); }
      continue;
    }
    if (k.UnicodeChar >= L'0' && k.UnicodeChar <= L'9') {
      if (len < 15) {
        buf[len++] = k.UnicodeChar; buf[len] = 0; Print(L"%c", k.UnicodeChar);
      }
    }
  }
  Print(L"\n");
  if (len == 0) return DefaultVal;
  return StrDecimalToUintn(buf);
}

// ----- PCI enumeration -----
STATIC
EFI_STATUS EnumeratePci(PCI_DEVICE_ENTRY **OutList, UINTN *OutCount) {
  EFI_STATUS Status;
  EFI_HANDLE *Handles = NULL;
  UINTN HandleCount = 0;

  Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiPciIoProtocolGuid, NULL, &HandleCount, &Handles);
  if (EFI_ERROR(Status)) return Status;

  PCI_DEVICE_ENTRY *list = AllocateZeroPool(sizeof(PCI_DEVICE_ENTRY) * HandleCount);
  if (!list) { FreePool(Handles); return EFI_OUT_OF_RESOURCES; }

  UINTN j = 0;
  for (UINTN i = 0; i < HandleCount; ++i) {
    EFI_PCI_IO_PROTOCOL *PciIo = NULL;
    Status = gBS->HandleProtocol(Handles[i], &gEfiPciIoProtocolGuid, (VOID**)&PciIo);
    if (EFI_ERROR(Status) || PciIo == NULL) continue;

    PCI_DEVICE_ENTRY *e = &list[j];
    e->Handle = Handles[i];
    e->PciIo = PciIo;
    PciIo->GetLocation(PciIo, &e->Segment, &e->Bus, &e->Device, &e->Function);

    // Read IDs & class
    PciIo->Pci.Read(PciIo, EfiPciIoWidthUint16, 0x00, 1, &e->VendorId);
    if (e->VendorId == 0xFFFF) continue; // not present
    PciIo->Pci.Read(PciIo, EfiPciIoWidthUint16, 0x02, 1, &e->DeviceId);

    PciIo->Pci.Read(PciIo, EfiPciIoWidthUint8,  0x0B, 1, &e->BaseClass);
    PciIo->Pci.Read(PciIo, EfiPciIoWidthUint8,  0x0A, 1, &e->SubClass);
    PciIo->Pci.Read(PciIo, EfiPciIoWidthUint8,  0x09, 1, &e->ProgIf);

    j++;
  }
  FreePool(Handles);

  if (j == 0) { FreePool(list); *OutList = NULL; *OutCount = 0; return EFI_NOT_FOUND; }

  // Sort for stable view
  PerformQuickSort(list, j, sizeof(PCI_DEVICE_ENTRY), CmpPci);
  *OutList = list;
  *OutCount = j;
  return EFI_SUCCESS;
}

STATIC
VOID DrawHeader(UINTN page, UINTN pages, UINTN count) {
  gST->ConOut->SetAttribute(gST->ConOut, EFI_WHITE | EFI_BACKGROUND_BLUE);
  Print(L" PCIe Browser  | Devices: %u  | Page %u/%u  ", (UINT32)count, (UINT32)(page+1), (UINT32)pages);
  gST->ConOut->SetAttribute(gST->ConOut, EFI_LIGHTGRAY | EFI_BACKGROUND_BLACK);
  Print(L"\nIdx  Seg:Bus:Dev.Func  Vend:Dev  Class             \n");
  Print(L"---- -----------------  --------  ------------------\n");
}

STATIC
VOID DrawHelp(VOID) {
  Print(L"\n[Keys] \x2191/\x2193 Move  PgUp/PgDn Page  Home/End Jump  D DumpCfg  R Read  W Write  Q Quit\n");
}

STATIC
VOID DrawList(PCI_DEVICE_ENTRY *list, UINTN count, UINTN sel, UINTN page) {
  gST->ConOut->ClearScreen(gST->ConOut);
  UINTN pages = (count + PAGE_LINES - 1) / PAGE_LINES;
  DrawHeader(page, pages ? pages : 1, count);

  UINTN start = page * PAGE_LINES;
  for (UINTN i = 0; i < PAGE_LINES; ++i) {
    UINTN idx = start + i;
    if (idx >= count) break;

    if (idx == sel) gST->ConOut->SetAttribute(gST->ConOut, EFI_BLACK | EFI_BACKGROUND_LIGHTGRAY);
    else            gST->ConOut->SetAttribute(gST->ConOut, EFI_LIGHTGRAY | EFI_BACKGROUND_BLACK);

    PCI_DEVICE_ENTRY *e = &list[idx];
    Print(L"%4u %04x:%02x:%02x.%x   %04x:%04x  %-18s\n",
          (UINT32)idx,
          (UINT32)e->Segment, e->Bus, e->Device, e->Function,
          e->VendorId, e->DeviceId,
          ClassName(e->BaseClass, e->SubClass));
  }
  gST->ConOut->SetAttribute(gST->ConOut, EFI_LIGHTGRAY | EFI_BACKGROUND_BLACK);
  DrawHelp();
}

STATIC
VOID DumpConfig(PCI_DEVICE_ENTRY *e) {
  UINT8 buf[256];
  e->PciIo->Pci.Read(e->PciIo, EfiPciIoWidthUint8, 0, sizeof(buf), buf);

  Print(L"\nConfig Space (256B) for %04x:%02x:%02x.%x  VID:PID=%04x:%04x\n",
        (UINT32)e->Segment, e->Bus, e->Device, e->Function, e->VendorId, e->DeviceId);

  for (UINTN i = 0; i < sizeof(buf); i += 16) {
    Print(L"%02x: ", (UINT32)i);
    for (UINTN j = 0; j < 16; ++j) {
      Print(L"%02x ", buf[i + j]);
    }
    Print(L" | ");
    for (UINTN j = 0; j < 16; ++j) {
      CHAR16 c = (buf[i + j] >= 32 && buf[i + j] < 127) ? (CHAR16)buf[i + j] : L'.';
      Print(L"%c", c);
    }
    Print(L"\n");
  }
  Print(L"\n");

  WaitForKey();
}

STATIC
EFI_STATUS DoConfigReadWrite(PCI_DEVICE_ENTRY *e, BOOLEAN WriteOp) {
  BOOLEAN abort = FALSE;
  UINTN widthSel = ReadDecFromConsole(L"Width bytes (1/2/4/8)?", 4, &abort);
  if (abort) return EFI_ABORTED;
  EFI_PCI_IO_PROTOCOL_WIDTH w;
  switch (widthSel) {
    case 1: w = EfiPciIoWidthUint8;  break;
    case 2: w = EfiPciIoWidthUint16; break;
    case 4: w = EfiPciIoWidthUint32; break;
    case 8: w = EfiPciIoWidthUint64; break;
    default: Print(L"Invalid width\n"); return EFI_INVALID_PARAMETER;
  }
  UINTN offset = ReadHexFromConsole(L"Config offset", 0x00, &abort);
  if (abort) return EFI_ABORTED;

  if (!WriteOp) {
    UINT64 val = 0;
    EFI_STATUS s = e->PciIo->Pci.Read(e->PciIo, w, (UINT32)offset, 1, &val);
    if (EFI_ERROR(s)) {
      Print(L"Read failed: %r\n", s);
      WaitForKey();
      return s;
    }
    Print(L"Value @CFG[0x%lx] = 0x%lx\n", offset, val);
  } else {
    UINT64 val = ReadHexFromConsole(L"Value to write", 0, &abort);
    if (abort) return EFI_ABORTED;
    EFI_STATUS s = e->PciIo->Pci.Write(e->PciIo, w, (UINT32)offset, 1, &val);
    if (EFI_ERROR(s)) {
      Print(L"Write failed: %r\n", s);
      WaitForKey();
      return s;
    }
    Print(L"Wrote 0x%lx -> CFG[0x%lx]\n", val, offset);
  }

  WaitForKey();

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS DoIoReadWrite(PCI_DEVICE_ENTRY *e, BOOLEAN WriteOp) {
  BOOLEAN abort = FALSE;
  UINTN widthSel = ReadDecFromConsole(L"Width bytes (1/2/4/8)?", 4, &abort);
  if (abort) return EFI_ABORTED;
  EFI_PCI_IO_PROTOCOL_WIDTH w;
  switch (widthSel) {
    case 1: w = EfiPciIoWidthUint8;  break;
    case 2: w = EfiPciIoWidthUint16; break;
    case 4: w = EfiPciIoWidthUint32; break;
    case 8: w = EfiPciIoWidthUint64; break;
    default: Print(L"Invalid width\n"); return EFI_INVALID_PARAMETER;
  }
  UINTN bar = ReadDecFromConsole(L"IO BarIndex (0-5)", 0, &abort);
  if (abort) return EFI_ABORTED;
  UINTN offset = ReadHexFromConsole(L"IO offset", 0x0, &abort);
  if (abort) return EFI_ABORTED;

  if (!WriteOp) {
    UINT64 val = 0;
    EFI_STATUS s = e->PciIo->Io.Read(e->PciIo, w, (UINT8)bar, offset, 1, &val);
    if (EFI_ERROR(s)) {
      Print(L"IO Read failed: %r\n", s);
      WaitForKey();
      return s;
    }
    Print(L"Value @IOBAR%u[0x%lx] = 0x%lx\n", (UINT32)bar, offset, val);
  } else {
    UINT64 val = ReadHexFromConsole(L"Value to write", 0, &abort);
    if (abort) return EFI_ABORTED;
    EFI_STATUS s = e->PciIo->Io.Write(e->PciIo, w, (UINT8)bar, offset, 1, &val);
    if (EFI_ERROR(s)) {
      Print(L"IO Write failed: %r\n", s);
      WaitForKey();
      return s;
    }
    Print(L"Wrote 0x%lx -> IOBAR%u[0x%lx]\n", val, (UINT32)bar, offset);
  }

  WaitForKey();

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS DoMmioReadWrite(PCI_DEVICE_ENTRY *e, BOOLEAN WriteOp) {
  BOOLEAN abort = FALSE;
  UINTN widthSel = ReadDecFromConsole(L"Width bytes (1/2/4/8)?", 4, &abort);
  if (abort) return EFI_ABORTED;
  EFI_PCI_IO_PROTOCOL_WIDTH w;
  switch (widthSel) {
    case 1: w = EfiPciIoWidthUint8;  break;
    case 2: w = EfiPciIoWidthUint16; break;
    case 4: w = EfiPciIoWidthUint32; break;
    case 8: w = EfiPciIoWidthUint64; break;
    default: Print(L"Invalid width\n"); return EFI_INVALID_PARAMETER;
  }
  UINTN bar = ReadDecFromConsole(L"MMIO BarIndex (0-5)", 0, &abort);
  if (abort) return EFI_ABORTED;
  UINTN offset = ReadHexFromConsole(L"MMIO offset", 0x0, &abort);
  if (abort) return EFI_ABORTED;

  if (!WriteOp) {
    UINT64 val = 0;
    EFI_STATUS s = e->PciIo->Mem.Read(e->PciIo, w, (UINT8)bar, offset, 1, &val);
    if (EFI_ERROR(s)) {
      Print(L"MMIO Read failed: %r\n", s);
      WaitForKey();
      return s;
    }
    Print(L"Value @MMIOBAR%u[0x%lx] = 0x%lx\n", (UINT32)bar, offset, val);
  } else {
    UINT64 val = ReadHexFromConsole(L"Value to write", 0, &abort);
    if (abort) return EFI_ABORTED;
    EFI_STATUS s = e->PciIo->Mem.Write(e->PciIo, w, (UINT8)bar, offset, 1, &val);
    if (EFI_ERROR(s)) {
      Print(L"MMIO Write failed: %r\n", s);
      WaitForKey();
      return s;
    }
    Print(L"Wrote 0x%lx -> MMIOBAR%u[0x%lx]\n", val, (UINT32)bar, offset);
  }

  WaitForKey();

  return EFI_SUCCESS;
}

STATIC
VOID DoReadWriteMenu(PCI_DEVICE_ENTRY *e, BOOLEAN WriteOp) {
  Print(L"\nTarget %04x:%02x:%02x.%x  VID:PID=%04x:%04x  [%s]\n",
        (UINT32)e->Segment, e->Bus, e->Device, e->Function,
        e->VendorId, e->DeviceId, ClassName(e->BaseClass, e->SubClass));
  Print(L"Space? (C=Config, I=IO, M=MMIO, ESC=cancel)\n");
  while (1) {
    EFI_INPUT_KEY k; UINT16 s;
    WaitForKey();
    if (!ReadKey(&k, &s)) continue;
    CHAR16 c = (CHAR16)((k.UnicodeChar >= L'a' && k.UnicodeChar <= L'z') ? (k.UnicodeChar - 32) : k.UnicodeChar);
    if (s == SCAN_ESC) break;
    if (c == L'C') { DoConfigReadWrite(e, WriteOp); break; }
    if (c == L'I') { DoIoReadWrite(e, WriteOp);     break; }
    if (c == L'M') { DoMmioReadWrite(e, WriteOp);   break; }
  }
}

// ----- Main entry -----
EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
)
{
  gBS->LocateProtocol(&gEfiSimpleTextInputExProtocolGuid, NULL, (VOID**)&gTextInEx);

  PCI_DEVICE_ENTRY *list = NULL;
  UINTN count = 0;
  EFI_STATUS Status = EnumeratePci(&list, &count);
  if (EFI_ERROR(Status) || count == 0) {
    Print(L"No PCI devices found: %r\n", Status);
    return Status;
  }

  UINTN sel = 0;
  UINTN page = 0;
  UINTN pages = (count + PAGE_LINES - 1) / PAGE_LINES;
  DrawList(list, count, sel, page);

  while (1) {
    EFI_INPUT_KEY k; UINT16 s;
    WaitForKey();
    if (!ReadKey(&k, &s)) continue;

    BOOLEAN redraw = FALSE;
    switch (s) {
      case SCAN_UP:
        if (sel > 0) { sel--; if (sel / PAGE_LINES != page) { page = sel / PAGE_LINES; } redraw = TRUE; }
        break;
      case SCAN_DOWN:
        if (sel + 1 < count) { sel++; if (sel / PAGE_LINES != page) { page = sel / PAGE_LINES; } redraw = TRUE; }
        break;
      case SCAN_HOME:
        sel = 0; page = 0; redraw = TRUE; break;
      case SCAN_END:
        sel = count - 1; page = sel / PAGE_LINES; redraw = TRUE; break;
      case SCAN_PAGE_UP:
        if (page > 0) { page--; sel = MIN(sel, page * PAGE_LINES + PAGE_LINES - 1); redraw = TRUE; }
        break;
      case SCAN_PAGE_DOWN:
        if (page + 1 < pages) { page++; sel = MAX(sel, page * PAGE_LINES); redraw = TRUE; }
        break;
      default:
        break;
    }

    CHAR16 c = k.UnicodeChar;
    if (c >= L'a' && c <= L'z') c -= 32; // uppercase

    if (c == L'Q') { break; }
    else if (c == L'D') { DumpConfig(&list[sel]); }
    else if (c == L'R') { DoReadWriteMenu(&list[sel], FALSE); }
    else if (c == L'W') { DoReadWriteMenu(&list[sel], TRUE); }
    else if (redraw) { /* handled below */ }

    if (redraw || c == L'D' || c == L'R' || c == L'W') {
      DrawList(list, count, sel, page);
    }
  }

  if (list) FreePool(list);
  return EFI_SUCCESS;
}
