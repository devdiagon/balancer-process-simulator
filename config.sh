sudo ip netns add ns_lb
sudo ip netns add ns_client
sudo ip netns add ns_isp1
sudo ip netns add ns_isp2

sudo ip link add veth_client type veth peer name veth_lb_in
sudo ip link add veth_lb_out1 type veth peer name veth_isp1
sudo ip link add veth_lb_out2 type veth peer name veth_isp2

sudo ip link set veth_client netns ns_client
sudo ip link set veth_lb_in netns ns_lb

sudo ip link set veth_lb_out1 netns ns_lb
sudo ip link set veth_isp1 netns ns_isp1

sudo ip link set veth_lb_out2 netns ns_lb
sudo ip link set veth_isp2 netns ns_isp2

sudo ip netns exec ns_client ip addr add 10.0.0.2/24 dev veth_client
sudo ip netns exec ns_client ip link set veth_client up

sudo ip netns exec ns_lb ip addr add 10.0.0.1/24 dev veth_lb_in
sudo ip netns exec ns_lb ip addr add 10.0.1.1/24 dev veth_lb_out1
sudo ip netns exec ns_lb ip addr add 10.0.2.1/24 dev veth_lb_out2
sudo ip netns exec ns_lb ip link set veth_lb_in up
sudo ip netns exec ns_lb ip link set veth_lb_out1 up
sudo ip netns exec ns_lb ip link set veth_lb_out2 up

sudo ip netns exec ns_isp1 ip addr add 10.0.1.2/24 dev veth_isp1
sudo ip netns exec ns_isp1 ip link set veth_isp1 up

sudo ip netns exec ns_isp2 ip addr add 10.0.2.2/24 dev veth_isp2
sudo ip netns exec ns_isp2 ip link set veth_isp2 up

sudo ip netns exec ns_lb sysctl -w net.ipv4.ip_forward=1

sudo ip netns exec ns_lb iptables -t nat -A PREROUTING \
  -i veth_lb_in -p tcp --dport 80 -j REDIRECT --to-port 8080

