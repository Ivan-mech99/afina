#include <afina/coroutine/Engine.h>

#include <setjmp.h>
#include <stdio.h>
#include <string.h>

namespace Afina {
namespace Coroutine {

Engine::~Engine() {
   if (StackBottom) {
      delete[] std::get<0>(idle_ctx->Stack);
      delete idle_ctx;
   }
   while (alive) {
      context *ctx = alive;
      delete[] std::get<0>(alive->Stack);
      delete ctx;
      alive = alive->next;
   }
   while (blocked) {
      context *ctx = blocked;
      delete[] std::get<0>(blocked->Stack);
      delete ctx;
      blocked = blocked->next;
   }
}

void Engine::Store(context &ctx) {
   char cur_addr;
   if(&cur_addr > ctx.Low) {
      ctx.Hight = &cur_addr;
   }
   else {
      ctx.Low = &cur_addr;
   }
   size_t mem_needed = ctx.Hight - ctx.Low;
   if (std::get<1>(ctx.Stack) < mem_needed) {
      std::get<1>(ctx.Stack) = mem_needed;
      delete [] std::get<0>(ctx.Stack);
      std::get<0>(ctx.Stack) = new char[mem_needed];
   }
   memcpy(std::get<0>(ctx.Stack), ctx.Low, mem_needed);
}

void Engine::Restore(context &ctx) {
   char cur_addr;
   while (&cur_addr <= ctx.Hight && &cur_addr >= ctx.Low) {
      Restore(ctx);
   }
   memcpy(ctx.Low, std::get<0>(ctx.Stack), std::get<1>(ctx.Stack));
   cur_routine = &ctx;
   longjmp(ctx.Environment, 1);
}

void Engine::yield() {
   context* ctx = alive;
   while (ctx != nullptr && ctx == cur_routine) {
      ctx = ctx -> next;
   }
   if (ctx != nullptr) {
      sched(ctx);
   }  
}

void Engine::sched(void *routine_) {
   if (routine_ == nullptr) {
      yield();
   }
   context *ctx = static_cast<context *>(routine_);
   if (cur_routine == ctx) {
      return;
   }
   if (cur_routine != idle_ctx) {
      if (setjmp(cur_routine->Environment) > 0) {
         return;
      }
      Store(*cur_routine);
   }
   Restore(*ctx);
}

} // namespace Coroutine
} // namespace Afina
