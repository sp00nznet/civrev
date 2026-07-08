// dispatch_tolerance.cpp -- HARVEST MODE (temporary bring-up scaffold).
//
// Based on templates/overlay/src/dispatch_tolerance.cpp. v0.8.0's
// ResolveIndirectFunction REX_FATALs on an indirect call to an unregistered
// guest address. Civ Rev's startup iterates a table of static-init / module-
// registration thunks (a dense run at 0x82E80xxx) that branch-discovery didn't
// place in functions, so each one fatals in turn.
//
// This override logs EVERY unique unregistered target (uncapped) and no-ops it,
// so ONE boot harvests the complete set. We then register all of them as
// [entrypoint.functions] hints and DELETE this file (proper fix = real
// functions, not tolerated no-ops). See PROGRESS.md Phase 4.
//
// Build: add to sources + LINKER:/force:multiple (we redefine an exported
// rexruntime symbol; project objects link first, so ours wins).

#include <cstdio>
#include <mutex>
#include <set>

#include <rex/ppc/context.h>
#include <rex/runtime.h>
#include <rex/system/function_dispatcher.h>

namespace {

std::mutex g_mu;
std::set<uint32_t> g_seen;

void NoopTrap(PPCContext& ctx, uint8_t* /*base*/) {
  uint32_t tgt = (uint32_t)ctx.last_indirect_target;
  std::lock_guard<std::mutex> lk(g_mu);
  if (g_seen.insert(tgt).second) {
    if (std::FILE* f = std::fopen("harvest.log", "a")) {
      std::fprintf(f, "0x%08X\n", tgt);
      std::fclose(f);
    }
  }
  ctx.r3.u64 = 0;
}

}  // namespace

namespace rex::runtime {

::PPCFunc* ResolveIndirectFunction(uint32_t guest_address) {
  if (Runtime* rt = Runtime::instance())
    if (FunctionDispatcher* d = rt->function_dispatcher())
      if (::PPCFunc* f = d->GetFunction(guest_address))
        return f;
  return &NoopTrap;
}

}  // namespace rex::runtime
