require 'socket'
require 'timeout'

def port_open?(ip, port, timeout = 0.5)
  Timeout.timeout(timeout) do
    begin
      # 指定したIPとポートに接続を試みる
      TCPSocket.new(ip, port).close
      true
    rescue Errno::ECONNREFUSED, Errno::EHOSTUNREACH, SocketError
      false
    end
  end
rescue Timeout::Error
  false
end

network_prefix = "192.168.3."
port = 22

puts "#{network_prefix}0/24 のポート #{port} をスキャン中..."

# 1から254までループ
(1..254).each do |i|
  target_ip = "#{network_prefix}#{i}"
  
  if port_open?(target_ip, port)
    puts "[○] #{target_ip}:#{port} は開放されています。"
  else
    # 閉鎖中のホストも表示したい場合はここを有効にしてください
    # puts "[ ] #{target_ip}:#{port} は応答なし。"
  end
end

puts "スキャン完了。"
