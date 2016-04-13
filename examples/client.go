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

type Args struct {
	A, B int
}

func main() {
	args := Args{1, 2}

	client, err := jsonrpc.Dial("tcp", URL)
	if err != nil {
		log.Fatal("dialing:", err)
	}

	{
		var reply string
		err = client.Call("sayHello", nil, &reply)
		if err != nil {
			log.Fatal("SayHello error:", err)
		}
		fmt.Printf("reply: %s\n", reply)
	}

	{
		reply := Args{}
		err = client.Call("swap", args, &reply)
		if err != nil {
			fmt.Println(err)
		}
		fmt.Printf("reply: A:%d B:%d\n", reply.A, reply.B)
	}

	//client.Close()
}
