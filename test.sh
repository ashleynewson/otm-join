export LC_ALL=C
yes | awk 'BEGIN{srand()}{$1=int(rand() * 10000000); print $1"\ta"}' | head -n10000000 | sort -u > a1
yes | awk 'BEGIN{srand()}{$1=int(rand() * 10000000); print $1"\tb"}' | head -n10000000 | sort > b1
time join -t$'\t' a1 b1 > r1
time ./otm-join a1 b1 > r2
sha256sum r1 r2
