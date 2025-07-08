import xml.etree.ElementTree as ET
import matplotlib.pyplot as plt
import csv

# Parse file XML
#tree = ET.parse("flowmon-output.xml")
tree = ET.parse("scratch/naufalCsma/flow-stats.xml")  # ← sesuaikan
root = tree.getroot()

flow_stats = []

# Ambil semua <Flow> di bawah <FlowStats>
for flow in root.findall(".//FlowStats/Flow"):
    try:
        tx_packets = int(flow.attrib.get('txPackets', '0'))
        rx_packets = int(flow.attrib.get('rxPackets', '0'))
        tx_bytes = int(flow.attrib.get('txBytes', '0'))
        rx_bytes = int(flow.attrib.get('rxBytes', '0'))
        delay_sum = float(flow.attrib.get('delaySum', '0').replace('ns', ''))
        jitter_sum = float(flow.attrib.get('jitterSum', '0').replace('ns', ''))
        time_first_tx = float(flow.attrib.get('timeFirstTxPacket', '0').replace('ns', ''))
        time_last_rx = float(flow.attrib.get('timeLastRxPacket', '0').replace('ns', ''))

        duration = (time_last_rx - time_first_tx) / 1e9  # ns to seconds
        throughput = (rx_bytes * 8) / duration / 1e6 if duration > 0 else 0  # Mbps
        pdr = rx_packets / tx_packets if tx_packets > 0 else 0
        avg_delay = (delay_sum / rx_packets) / 1e6 if rx_packets > 0 else 0  # ms
        avg_jitter = (jitter_sum / rx_packets) / 1e6 if rx_packets > 0 else 0  # ms
        packet_loss = (tx_packets - rx_packets) / tx_packets if tx_packets > 0 else 0

        flow_stats.append({
            "flowId": flow.attrib.get("flowId", "N/A"),
            "throughput": throughput,
            "pdr": pdr,
            "delay": avg_delay,
            "jitter": avg_jitter,
            "loss": packet_loss
        })

    except Exception as e:
        print(f"⚠️ Skipping flow due to error: {e}")

# Simpan ke CSV
with open("scratch/naufalCsma/flow-stats.csv", "w", newline="") as csvfile:
    fieldnames = ["flowId", "throughput", "pdr", "delay", "jitter", "loss"]
    writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
    writer.writeheader()
    for stat in flow_stats:
        writer.writerow(stat)

# Visualisasi
metrics = ["throughput", "pdr", "delay", "jitter", "loss"]
titles = {
    "throughput": "Throughput (Mbps)",
    "pdr": "Packet Delivery Ratio",
    "delay": "Average Delay (ms)",
    "jitter": "Average Jitter (ms)",
    "loss": "Packet Loss Ratio"
}

for metric in metrics:
    values = [flow[metric] for flow in flow_stats]
    plt.figure(figsize=(10, 4))
    plt.bar(range(len(values)), values)
    plt.title(titles[metric])
    plt.xlabel("Flow Index")
    plt.ylabel(titles[metric])
    plt.grid(True, linestyle='--', alpha=0.6)
    plt.tight_layout()
    plt.savefig(f"scratch/naufalCsma/{metric}.png")
    plt.close()

print("✅ Analisis selesai. Grafik disimpan sebagai PNG.")
print(f"✅ Total flow valid yang dianalisis: {len(flow_stats)}")
print("📄 Data lengkap disimpan ke: flow-stats.csv")

