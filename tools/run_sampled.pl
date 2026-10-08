# Run a command for N seconds, then sample all its threads (macOS `sample`) into <log>.sample and kill it.
# usage: perl tools/run_sampled.pl <wait_seconds> <log> <cmd...>
my ($wait, $log, @cmd) = @ARGV;
my $pid = fork();
if ($pid == 0) { open(STDOUT, '>', $log); open(STDERR, '>&', \*STDOUT); exec @cmd; }
sleep $wait;
system("sample $pid 2 -file $log.sample > /dev/null 2>&1");
kill 'KILL', $pid; waitpid($pid, 0);
