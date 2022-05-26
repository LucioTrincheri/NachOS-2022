/// Routines to manage address spaces (memory for executing user programs).
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "address_space.hh"
#include "executable.hh"
#include "threads/system.hh"
#include <algorithm>
#include <stdint.h>

#include <string.h>

#include "lib/bitmap.hh"
#include "mmu.hh" //NUM_PHYS_PAGES

#define DEMAND_LOADING = 1

Bitmap *usedPages = new Bitmap(NUM_PHYS_PAGES);

// Estas funciones pueden reemplazar lo que se realizan en las lineas 107 a 116 y 131 a 140.
// No se pueden realizar las llamas a estas funciones ya que por lo aparente, la MMU traduce tambien estas llamadas.
// Y los calculos de offsets se traducen de manera incorrecta.
const uint32_t dataBytes(const uint32_t dataAddrStart, const uint32_t dataAddrEnd, const uint32_t pageAddrStart, const uint32_t pageAddrEnd){
    const uint32_t data_bytes = std::min(dataAddrEnd, pageAddrEnd) - std::max(dataAddrStart, pageAddrStart) + 1;
    ASSERT(data_bytes > 0 && data_bytes <= PAGE_SIZE);
    return data_bytes;
}
const uint32_t memoryOffset(const uint32_t dataAddrStart, const uint32_t pageAddrStart){
    const uint32_t memory_offset = (dataAddrStart > pageAddrStart) ? (dataAddrStart - pageAddrStart) : 0;
    ASSERT(memory_offset < PAGE_SIZE);
    return memory_offset;
}
const uint32_t dataOffset(const uint32_t dataSize, const uint32_t dataAddrStart, const uint32_t pageAddrStart){
    const uint32_t data_offset = (pageAddrStart > dataAddrStart) ? (pageAddrStart - dataAddrStart) : 0;
    ASSERT(data_offset < dataSize);
    return data_offset;
}
//bool
//AddressSpace::isMemoryFull(){
//    return fullMemory;
//}
/// First, set up the translation from program memory to physical memory.
/// For now, this is really simple (1:1), since we are only uniprogramming,
/// and we have a single unsegmented page table.
AddressSpace::AddressSpace(OpenFile *executable_file)
{
    ASSERT(executable_file != nullptr);

    exe = new Executable(executable_file);
    
    // How big is address space?

    size = exe->GetSize() + USER_STACK_SIZE;
      // We need to increase the size to leave room for the stack.
    numPages = DivRoundUp(size, PAGE_SIZE);
    loadedPages = 0;
    size = numPages * PAGE_SIZE;

    if (numPages > usedPages->CountClear()) {
        fullMemory = true;
        return;
    }
    fullMemory = false;

    DEBUG('p', "Initializing address space, num pages %u, size %u\n",
          numPages, size);

    // Accedo a la MMU antes ya que necesito hacer memset en la posición de memoria durante el TranslationEntry
    char *mainMemory = machine->GetMMU()->mainMemory;

    codeSize = exe->GetCodeSize();
    codeAddrStart = exe->GetCodeAddr();
    codeAddrEnd = codeAddrStart + codeSize - 1;

    initDataSize = exe->GetInitDataSize();
    initDataAddrStart = exe->GetInitDataAddr();
    initDataAddrEnd = initDataAddrStart + initDataSize - 1;

    pageTable = new TranslationEntry[numPages];
#ifndef DEMAND_LOADING
    for (unsigned i = 0; i < numPages; i++) {
        LoadPage(i);        
    }
    DEBUG('a', "Initialized user address space\n");
    DEBUG('p', "Initialized user page table\n");
    if (debug.IsEnabled('p')) {
        usedPages->Print();
    }
#else
    DEBUG('e', "Antes del for de inicializacion\n");
    
    for (unsigned i = 0; i < numPages; i++) {
        //const uint32_t pageAddrStart = i * PAGE_SIZE;
        //const uint32_t pageAddrEnd = pageAddrStart + PAGE_SIZE - 1;
        pageTable[i].virtualPage  = i;
        pageTable[i].physicalPage = -1;
        pageTable[i].valid        = true;
        //pageTable[i].readOnly     = codeSize > 0 && pageAddrStart <= codeAddrEnd && pageAddrEnd >= codeAddrStart && !(codeAddrEnd < pageAddrEnd);
        //pageTable[i].use          = false;
        //pageTable[i].dirty        = false;
    }
#endif
}

AddressSpace::~AddressSpace()
{
    // Liberamos los marcos utilizados por el proceso
    for(unsigned p = 0; p < loadedPages; p++) {
        usedPages->Clear(pageTable[p].physicalPage);
    }
    delete [] pageTable;
    DEBUG('p', "Deleted page table\n");
    if (debug.IsEnabled('p')) {
        usedPages->Print();
    }
    DEBUG('a', "Deleted user address space\n");
}
void
AddressSpace::LoadPage(int vpn) {
    const uint32_t pageAddrStart = vpn * PAGE_SIZE;
    const uint32_t pageAddrEnd = pageAddrStart + PAGE_SIZE - 1;
    loadedPages++;
    const int physical = usedPages->Find();
    char *mainMemory = machine->GetMMU()->mainMemory;
    DEBUG('p', "Luego de la copia\n");

    OpenFile * hola = fileSystem->Open("userland/shell");
    Executable * exe1 = new Executable(hola);

    pageTable[vpn].virtualPage  = vpn;
    pageTable[vpn].physicalPage = physical;
    pageTable[vpn].valid        = true;
    pageTable[vpn].readOnly     = codeSize > 0 && pageAddrStart <= codeAddrEnd && pageAddrEnd >= codeAddrStart && !(codeAddrEnd < pageAddrEnd);
    pageTable[vpn].use          = false;
    pageTable[vpn].dirty        = false;

    mainMemory = machine->GetMMU()->mainMemory;
    DEBUG('p', "Zeroing out virtual page %u, physical page: %u\n", vpn, physical);
    memset(&mainMemory[physical * PAGE_SIZE], 0, PAGE_SIZE);
    DEBUG('p', "luego de setear en 0 la pagina en la memoria fisica\n");

    if (codeSize > 0 && pageAddrStart <= codeAddrEnd && pageAddrEnd >= codeAddrStart) {
        DEBUG('p', "Entro al primer if\n");
        const uint32_t code_bytes = std::min(codeAddrEnd, pageAddrEnd) - std::max(codeAddrStart, pageAddrStart) + 1;

        const uint32_t memory_offset = (codeAddrStart > pageAddrStart) ? (codeAddrStart - pageAddrStart) : 0;

        const uint32_t code_offset = (memory_offset != 0) ? 0 : (pageAddrStart - codeAddrStart);

        DEBUG('p', "Copying code block from 0x%X to 0x%X (%u bytes) into physical page %u\n",
            code_offset, code_offset + code_bytes - 1, code_bytes, physical);
        DEBUG('e', "Values dt.data_bytes = %u, dt.memory_offset = %u, dt.data_offset = %u\n", code_bytes, memory_offset, code_offset);
        DEBUG('e', "tamanio de exe %d\n", exe->GetSize());
        DEBUG('e', "main memory %s\n", mainMemory);
        DEBUG('e', "main memory completa %s\n", &mainMemory[physical * PAGE_SIZE + memory_offset]);
        exe->ReadCodeBlock(&mainMemory[physical * PAGE_SIZE + memory_offset], code_bytes, code_offset);
        DEBUG('p', "Termino el primer if\n");
    }
    
    /*
    if (codeSize > 0 && pageAddrStart <= codeAddrEnd && pageAddrEnd >= codeAddrStart) {
        const uint32_t data_bytes = dataBytes(codeAddrStart, codeAddrEnd, pageAddrStart, pageAddrEnd);
        const uint32_t memory_offset = memoryOffset(codeAddrStart, pageAddrStart);
        const uint32_t data_offset = dataOffset(codeSize, codeAddrStart, pageAddrStart);
        DEBUG('e', "Values dt.data_bytes = %u, dt.memory_offset = %u, dt.data_offset = %u\n", data_bytes, memory_offset, data_offset);
        DEBUG('p', "Copying code block from 0x%X to 0x%X (%u bytes) into physical page %u\n",
        exe.ReadCodeBlock(&mainMemory[physical * PAGE_SIZE + memory_offset], data_bytes, data_offset);
    */
    if (initDataSize > 0 && pageAddrStart <= initDataAddrEnd && pageAddrEnd >= initDataAddrStart) {
        DEBUG('p', "Entro al segundo if\n");
        const uint32_t data_bytes = std::min(initDataAddrEnd, pageAddrEnd) - std::max(initDataAddrStart, pageAddrStart) + 1;

        const uint32_t memory_offset = (initDataAddrStart > pageAddrStart) ? (initDataAddrStart - pageAddrStart) : 0;

        const uint32_t data_offset = (memory_offset != 0) ? 0 : (pageAddrStart - initDataAddrStart);
        
        exe->ReadDataBlock(&mainMemory[physical * PAGE_SIZE + memory_offset], data_bytes, data_offset);
        DEBUG('p', "Termino el segundo if\n");
    }
    /*
    if (initDataSize > 0 && pageAddrStart <= initDataAddrEnd && pageAddrEnd >= initDataAddrStart) {
        const uint32_t data_bytes = dataBytes(initDataAddrStart, initDataAddrEnd, pageAddrStart, pageAddrEnd);
        const uint32_t memory_offset = memoryOffset(initDataAddrStart, pageAddrStart);
        const uint32_t data_offset = dataOffset(codeSize, initDataAddrStart, pageAddrStart);
        DEBUG('e', "Values dt.data_bytes = %u, dt.memory_offset = %u, dt.data_offset = %u\n", data_bytes, memory_offset, data_offset);
        DEBUG('p', "Copying code block from 0x%X to 0x%X (%u bytes) into physical page %u\n",
            data_offset, data_offset + data_bytes - 1, data_bytes, physical);
        exe.ReadCodeBlock(&mainMemory[physical * PAGE_SIZE + memory_offset], data_bytes, data_offset);
    */
}


/// Set the initial values for the user-level register set.
///
/// We write these directly into the â€œmachineâ€ registers, so that we can
/// immediately jump to user code.  Note that these will be saved/restored
/// into the `currentThread->userRegisters` when this thread is context
/// switched out.
void
AddressSpace::InitRegisters()
{
    for (unsigned i = 0; i < NUM_TOTAL_REGS; i++) {
        machine->WriteRegister(i, 0);
    }

    // Initial program counter -- must be location of `Start`.
    machine->WriteRegister(PC_REG, 0);

    // Need to also tell MIPS where next instruction is, because of branch
    // delay possibility.
    machine->WriteRegister(NEXT_PC_REG, 4);

    // Set the stack register to the end of the address space, where we
    // allocated the stack; but subtract off a bit, to make sure we do not
    // accidentally reference off the end!
    machine->WriteRegister(STACK_REG, numPages * PAGE_SIZE - 16);
    DEBUG('a', "Initializing stack register to %u\n",
          numPages * PAGE_SIZE - 16);
}

/// On a context switch, save any machine state, specific to this address
/// space, that needs saving.
///
/// For now, nothing!
void
AddressSpace::SaveState()
{}

TranslationEntry *
AddressSpace::GetPageTable() {
    return pageTable;
}

/// On a context switch, restore the machine state so that this address space
/// can run.
///
/// For now, tell the machine where to find the page table.
void
AddressSpace::RestoreState()
{
    // Comentamos todo lo de la tabla de paginacion para que se pueda usar la TLB
    // machine->GetMMU()->pageTable     = pageTable;
    // machine->GetMMU()->pageTableSize = numPages;
    for(unsigned int i=0; i<TLB_SIZE; i++) {
        machine->GetMMU()->tlb[i].valid = false;
    }
}
