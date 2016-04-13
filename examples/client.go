/*
 * yubo@yubo.org
 * 2016-04-13
 */

package main

import (
	"fmt"
	"log"
	"net/rpc/jsonrpc"
)

const (
	URL = "127.0.0.1:1234"
)

func main() {
	var reply string

	client, err := jsonrpc.Dial("tcp", URL)
	if err != nil {
		log.Fatal("dialing:", err)
	}

	err = client.Call("sayHello", nil, &reply)
	if err != nil {
		log.Fatal("SayHello error:", err)
	}
	fmt.Printf("reply: %s\n", reply)

	err = client.Call("exit", nil, &reply)
	if err != nil {
		log.Fatal("exit error:", err)
	}
	fmt.Printf("reply: %s\n", reply)

	//client.Close()
}
