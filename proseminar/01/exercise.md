# Assignment 01

## Exercise 1

- Study how to submit jobs in SLURM, how to check their state and how to cancel them.
    - submit in two ways: via script (recommended) or as a command line
    - for the script see *job.slurm*
    - state check: `squ` short for `squeue -U $USER`
    - cancellation: scancel *<job_id>*
- In your opionion, what are the 5 most important parameters available when submitting a job and why? What are possible settings of these parameters, and what effect do they have?
    - *--job-name <type_string>*: makes it easier to find your job in squeue
    - *--output <type_file>*: redirect output stream (stdout) to any file for validating what's happening
    - *--error <type_file>*: redirect error stream (stderr) for debugging purposes
    - *--mail-user <email_address>*: for jobs that take longer, you can exit the ssh connection and leave the job. When something is happening (job is running, ended, etc.) you get notified
    - *--time=<type_time>*: sometimes programs might be corrupt and end up taking longer as wanted, then this can be very handy. <type_time> must minutes, days-hours, or [[days-]hours:]minutes[:seconds]
- How do you run your program in parallel? What environment setup is required?
    - use *--ntasks=<n_tasks>* to specify the number of task slots
    - with *--ntasks-per-node=<n_tasks_per_node>* (trivial explaination)
    - IMPORTANT: use `srun ./whatever` in your script, otherwise it will only execute once.
    - Also, your program needs to be written in a parallel manner to work properly. Otherwise, it will just execute #N number of times

## Exercise 2

