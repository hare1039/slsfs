import csv
import sys
import statistics
import collections
import json

if __name__ == "__main__":
    with open("proxy-report.json") as fp:
        proxy_report = json.load(fp)

    savename = sys.argv[1]
    csvs = sys.argv[2:]

    final_table = []
    filename_dict = {}
    col = 0

    client_run_duration = [""]
    summary_table_name  = [""]
    summary_table_value = [""]
    testname = ""
    dist = True

    all_latency = []
    cold_start_latency = []
    for filename in csvs:
        with open(filename, "r") as f:
            csvreader = csv.DictReader(f)
            rows = []
            for row in csvreader:
                rows.append(row)

            col = len(rows)
            recorded_clients = list([x for x in rows[0].keys() if "client" in x])

            for key in recorded_clients:
                for row in rows:
                    all_latency.append(float(row[key]))
                cold_start_latency.append(float(rows[0][key]))

            if not final_table:
                final_table = [x for x in rows[0].keys() if "client" not in x]

            edited_rows = []
            for row in rows:
                m = {}
                for k in row.keys():
                    m[k + "|" + filename] = row[k]
                edited_rows.append(m)

            for k in recorded_clients:
                final_table.append(k + "|" + filename)

            filename_dict[filename] = edited_rows

            for row in rows:
                for x in rows[0].keys():
                    if "client" not in x:
                        if row[x] == "":
                            continue
                        if x == "summary":
                            if "dist" in row[x]:
                                dist = False

                            if dist:
                                summary_table_name.append(row[x])
                        elif x == "":
                            if dist:
                                summary_table_value.append(row[x])
                        else:
                            testname = x
                            client_run_duration.append(row[x])

    try:
        average_runtime = statistics.mean([int(x[:-2]) for x in client_run_duration if x != ""])
    except:
        average_runtime = 0

    client_run_duration.insert(1, "")
    client_run_duration.insert(1, "{:.2f}s".format(average_runtime/1000000000))

    summary_table_name.insert(1, "IOPS(all)")
    summary_table_value.insert(1, len(all_latency) / (average_runtime/1000000000))

    summary_table_name.insert(1, "IOPS(hot)")
    avg_cold = statistics.mean(cold_start_latency) / 1000000000
    summary_table_value.insert(1, len(all_latency) / ((average_runtime/1000000000) - avg_cold))

    summary_table_name.insert(1, "TotalReqs")
    summary_table_value.insert(1, len(all_latency))

    summary_table_name.append("")
    summary_table_value.append("")

    summary_table_name.append("DF Stat")
    summary_table_value.append("")

    summary_table_name.append("Total DF")
    summary_table_value.append(proxy_report["started_df"])

    util = []
    startup_time = []
    for df in proxy_report["df"]:
        util.append(df["finished_job_count"] / (df["duration"] / 1000000000))
        startup_time.append(df["start_duration"])

    summary_table_name.append("DF Start (s)")
    summary_table_value.append(statistics.mean(startup_time) / 1000000000)

    summary_table_name.append("Avg Util=(finished job/duration)")
    summary_table_value.append(statistics.mean(util))

    summary_table_name.append("Max Util")
    summary_table_value.append(max(util))

    average_latency = statistics.mean([float(x) for x in all_latency])
    average_latency_stdev = statistics.stdev([float(x) for x in all_latency])

    distribution = collections.defaultdict(lambda: 0)
    for x in all_latency:
        distribution[int(x / 1000000)] += 1

    summary_table_name.append("")
    summary_table_value.append("")
    summary_table_name.append("dist bucket(ms)")
    summary_table_value.append("")

    k = sorted(distribution.keys())
    for key in k:
        summary_table_name.append(key)
        summary_table_value.append(distribution[key])

    with open(savename, "w") as f:
        writer = csv.DictWriter(f, fieldnames=final_table, extrasaction="ignore")

        writer.writeheader()

        for cindex in range(col):
            thisrow = {}
            for filename in csvs:
                rows = filename_dict[filename]
                thisrow.update(rows[cindex])
                thisrow.update({
                    testname: client_run_duration[cindex] if cindex < len(client_run_duration) else "",
                    "summary": summary_table_name[cindex] if cindex < len(summary_table_name) else "",
                    "": summary_table_value[cindex] if cindex < len(summary_table_value) else ""
                })

            writer.writerow(thisrow)
