Thread Switching

if (cur != idle_thread)
    list_push_back (&ready_list, &cur->elem);
  cur->status = THREAD_READY;
  schedule ();
  intr_set_level (old_level);

1. cur thread and next thread are both calling schedule() that time
2. next thread is goint to turn the intr back to old_level
